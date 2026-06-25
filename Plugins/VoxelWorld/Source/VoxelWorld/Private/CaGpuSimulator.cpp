#include "CaGpuSimulator.h"
#include "CaCellPacking.h"
#include "CaMargolusShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "GlobalShader.h"
#include "RenderingThread.h"

// M-10 Phase 2：同步阻塞版正式實作（對應 Godot CaGpuSimulator.cs 的 Submit()+Sync()，
// docs/plan-m10-gpu-ca.md）。Phase 0/1 已經把所有「UE5 RDG 怎麼接」的坑都踩過一輪
// （shader type 載入時機、跨 thread 建構/Lock 限制、shader parameter 宣告寫法），這裡
// 直接套用同一套已驗證的模式，不是重新摸索。

// 對應 Godot CaGpuSimulator.cs:204-207 的 dispatch group 數計算：每個 invocation 處理一個
// 2x2x2 block（2 格/軸），每個 thread group 是 4x4x4 個 invocation（numthreads(4,4,4)，
// CaMargolus.usf），所以每個 thread group 覆蓋 8 格/軸。groups = ceil(ceil(Size/2)/4)。
// （跟 M10MargolusTest.cpp 裡同名函式重複——Phase 1 驗證碼跟 Phase 2 正式實作刻意分開，
// 各自獨立、不互相依賴；這個公式只有 2 行，重複比硬拉一個共用標頭檔的耦合成本更低。）
static int32 ComputeMargolusGroups(int32 Size)
{
    const int32 HalfSize = (Size + 1) / 2;
    return FMath::Max((HalfSize + 3) / 4, 1);
}

FCaGpuSimulator::~FCaGpuSimulator()
{
    Release();
}

bool FCaGpuSimulator::Initialize()
{
    bAvailable = GIsRHIInitialized;
    return bAvailable;
}

void FCaGpuSimulator::SetOrigin(int32 OX, int32 OY, int32 OZ)
{
    OriginX = OX;
    OriginY = OY;
    OriginZ = OZ;
}

bool FCaGpuSimulator::Upload(const TArray<uint32>& PackedCells, int32 InW, int32 InH, int32 InD)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_CaGpu_Upload);
    if (!bAvailable) return false;

    // 對應 Godot CaGpuSimulator.cs:150-151,183：PhysMask = 0x300（bits 8-9），
    // 全部都是靜態/空格的話直接回 false，呼叫端可以跳過整個 Simulate+Download。
    const uint32 PhysMask = 0x300u;
    bool bHasPhysics = false;
    for (uint32 Cell : PackedCells)
    {
        if ((Cell & PhysMask) != 0u) { bHasPhysics = true; break; }
    }
    if (!bHasPhysics) return false;

    CurW = InW;
    CurH = InH;
    CurD = InD;

    // 同 Phase 0/1 的教訓：GPU 資源（這裡是建 RDG buffer + QueueBufferUpload）要在
    // render thread 上做，搬進 ENQUEUE_RENDER_COMMAND，透過參照寫回 PooledBuffer。
    TRefCountPtr<FRDGPooledBuffer>* PooledBufferPtr = &PooledBuffer;
    const int32 NumCells = PackedCells.Num();

    ENQUEUE_RENDER_COMMAND(CaGpuUpload)(
        [PackedCells, NumCells, PooledBufferPtr](FRHICommandListImmediate& RHICmdList)
        {
            FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("CaGpuUpload"));

            FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(
                FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumCells), TEXT("CaGpuBuffer"));

            GraphBuilder.QueueBufferUpload(Buffer, PackedCells.GetData(), PackedCells.Num() * sizeof(uint32));

            // QueueBufferExtraction：把這次的 RDG buffer 轉成跨 frame 持續存在的 pooled
            // buffer，下一次 Simulate()/Download() 呼叫時用 RegisterExternalBuffer() 接回來。
            GraphBuilder.QueueBufferExtraction(Buffer, PooledBufferPtr);

            GraphBuilder.Execute();
        });

    FlushRenderingCommands();
    return true;
}

void FCaGpuSimulator::Simulate(uint32 RngSeed)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_CaGpu_Simulate);
    if (!bAvailable || !PooledBuffer.IsValid()) return;

    TRefCountPtr<FRDGPooledBuffer> LocalPooled = PooledBuffer;
    TRefCountPtr<FRDGPooledBuffer>* PooledBufferPtr = &PooledBuffer;
    const int32 W = CurW, H = CurH, D = CurD;
    const int32 Groups = ComputeMargolusGroups(FMath::Max3(W, H, D));

    ENQUEUE_RENDER_COMMAND(CaGpuSimulate)(
        [LocalPooled, PooledBufferPtr, RngSeed, W, H, D, Groups](FRHICommandListImmediate& RHICmdList)
        {
            FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("CaGpuSimulate"));

            FRDGBufferRef Buffer = GraphBuilder.RegisterExternalBuffer(LocalPooled);

            TShaderMapRef<FCaMargolusCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

            // 對應 Godot CaGpuSimulator.cs:217-230：Phase 0/1 各 dispatch 一次。
            for (int32 Phase = 0; Phase < 2; ++Phase)
            {
                FCaMargolusCS::FParameters* Params = GraphBuilder.AllocParameters<FCaMargolusCS::FParameters>();
                Params->Cells  = GraphBuilder.CreateUAV(Buffer);
                Params->W      = W;
                Params->H      = H;
                Params->D      = D;
                Params->Phase  = Phase;
                Params->Rng    = RngSeed ^ static_cast<uint32>(Phase * 0xDEADBEEF);
                Params->ZFrom  = 0;
                Params->ZCount = D;

                FComputeShaderUtils::AddPass(
                    GraphBuilder, RDG_EVENT_NAME("CaGpuDispatch"), ComputeShader, Params,
                    FIntVector(Groups, Groups, Groups));
            }

            // 模擬完的結果要再存回 pooled buffer，下一次 Simulate()/Download() 才讀到最新狀態。
            GraphBuilder.QueueBufferExtraction(Buffer, PooledBufferPtr);

            GraphBuilder.Execute();
        });

    FlushRenderingCommands();
}

void FCaGpuSimulator::Download(FSetCellCallback SetCellCb)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_CaGpu_Download);
    if (!bAvailable || !PooledBuffer.IsValid()) return;

    TRefCountPtr<FRDGPooledBuffer> LocalPooled = PooledBuffer;
    const int32 W = CurW, H = CurH, D = CurD;
    const uint32 ByteCount = static_cast<uint32>(W * H * D) * sizeof(uint32);

    // 同 Phase 0/1 教訓：FRHIGPUBufferReadback 要在 render thread 建構。
    FRHIGPUBufferReadback* Readback = nullptr;

    ENQUEUE_RENDER_COMMAND(CaGpuDownload)(
        [LocalPooled, ByteCount, &Readback](FRHICommandListImmediate& RHICmdList)
        {
            Readback = new FRHIGPUBufferReadback(TEXT("CaGpuReadback"));

            FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("CaGpuDownload"));
            FRDGBufferRef Buffer = GraphBuilder.RegisterExternalBuffer(LocalPooled);
            AddEnqueueCopyPass(GraphBuilder, Readback, Buffer, ByteCount);
            GraphBuilder.Execute();
        });

    FlushRenderingCommands();

    // Poll() 不要求 render thread（跟 Wait() 不同，見 RHIResources.h FRHIGPUFence 文件
    // 註解），這個忙等迴圈留在 game thread——Phase 0 的「不要無止盡忙等」警告這裡先不處理
    // （Phase 2 範圍內單次呼叫可接受，Phase 4 量效能時再評估）。
    const double StartTime = FPlatformTime::Seconds();
    while (!Readback->IsReady())
    {
        FPlatformProcess::Sleep(0.001f);
        if (FPlatformTime::Seconds() - StartTime > 5.0)
        {
            ENQUEUE_RENDER_COMMAND(CaGpuDownloadCleanup)([Readback](FRHICommandListImmediate&) { delete Readback; });
            FlushRenderingCommands();
            return;
        }
    }

    // Lock()/Unlock() 一樣要求 render thread（LockStagingBuffer_RenderThread()）。
    ENQUEUE_RENDER_COMMAND(CaGpuDownloadVerify)(
        [ByteCount, Readback, W, H, SetCellCb](FRHICommandListImmediate&)
        {
            const uint32* Data = static_cast<const uint32*>(Readback->Lock(ByteCount));
            const int32 NumCells = static_cast<int32>(ByteCount / sizeof(uint32));

            for (int32 i = 0; i < NumCells; ++i)
            {
                if (!CaCellPacking::IsDirty(Data[i])) continue;

                const int32 z = i / (H * W);
                const int32 rem = i % (H * W);
                const int32 y = rem / W;
                const int32 x = rem % W;

                SetCellCb(x, y, z, Data[i]);
            }

            Readback->Unlock();
            delete Readback;
        });

    FlushRenderingCommands();
}

// ── Phase 5：async readback ───────────────────────────────────────────────────

bool FCaGpuSimulator::HasPendingAsync() const
{
    return AsyncReadbackInFlight.load(std::memory_order_relaxed) != nullptr;
}

bool FCaGpuSimulator::BeginAsync(TArray<uint32> PackedCells, int32 InW, int32 InH, int32 InD, uint32 RngSeed)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_CaGpu_BeginAsync);
    if (!bAvailable) return false;

    const uint32 PhysMask = 0x300u;
    bool bHasPhysics = false;
    for (uint32 Cell : PackedCells)
        if ((Cell & PhysMask) != 0u) { bHasPhysics = true; break; }
    if (!bHasPhysics) return false;

    // 若上一幀的 readback 還沒被 TryCollectAsync 收取，丟棄舊的（1 幀舊的資料直接放棄）。
    FRHIGPUBufferReadback* Old = AsyncReadbackInFlight.exchange(nullptr, std::memory_order_acquire);
    if (Old)
    {
        ENQUEUE_RENDER_COMMAND(CaGpuDropOldReadback)([Old](FRHICommandListImmediate&) { delete Old; });
        // 不 flush：fire and forget，drop command 會在新 BeginAsync command 之前執行（FIFO）
    }

    AsyncW       = InW;
    AsyncH       = InH;
    AsyncD       = InD;
    AsyncCornerX = OriginX - InW / 2;
    AsyncCornerY = OriginY - InH / 2;
    AsyncCornerZ = OriginZ - InD / 2;

    TRefCountPtr<FRDGPooledBuffer>*           PooledBufferPtr    = &PooledBuffer;
    std::atomic<FRHIGPUBufferReadback*>*      AsyncReadbackPtr   = &AsyncReadbackInFlight;
    const int32 NumCells   = PackedCells.Num();
    const uint32 ByteCount = static_cast<uint32>(NumCells) * sizeof(uint32);
    const int32 Groups     = ComputeMargolusGroups(FMath::Max3(InW, InH, InD));

    ENQUEUE_RENDER_COMMAND(CaGpuBeginAsync)(
        [PackedCells = MoveTemp(PackedCells), NumCells, ByteCount,
         PooledBufferPtr, AsyncReadbackPtr, RngSeed, InW, InH, InD, Groups]
        (FRHICommandListImmediate& RHICmdList)
        {
            // Graph 1：Upload（同 FCaGpuSimulator::Upload）
            TRefCountPtr<FRDGPooledBuffer> LocalPooled;
            {
                FRDGBuilder GB(RHICmdList, RDG_EVENT_NAME("CaGpuAsync_Upload"));
                FRDGBufferRef Buf = GB.CreateBuffer(
                    FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumCells), TEXT("CaGpuAsyncBuf"));
                GB.QueueBufferUpload(Buf, PackedCells.GetData(), ByteCount);
                GB.QueueBufferExtraction(Buf, &LocalPooled);
                GB.Execute();
            }

            // Graph 2：Simulate（Margolus Phase 0+1）+ BeginReadbackCopy
            {
                FRDGBuilder GB(RHICmdList, RDG_EVENT_NAME("CaGpuAsync_Sim"));
                FRDGBufferRef Buf = GB.RegisterExternalBuffer(LocalPooled);

                TShaderMapRef<FCaMargolusCS> CS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
                for (int32 P = 0; P < 2; ++P)
                {
                    FCaMargolusCS::FParameters* Params = GB.AllocParameters<FCaMargolusCS::FParameters>();
                    Params->Cells  = GB.CreateUAV(Buf);
                    Params->W      = InW;
                    Params->H      = InH;
                    Params->D      = InD;
                    Params->Phase  = P;
                    Params->Rng    = RngSeed ^ static_cast<uint32>(P * 0xDEADBEEF);
                    Params->ZFrom  = 0;
                    Params->ZCount = InD;
                    FComputeShaderUtils::AddPass(GB, RDG_EVENT_NAME("CaGpuDispatch"), CS, Params,
                        FIntVector(Groups, Groups, Groups));
                }
                GB.QueueBufferExtraction(Buf, PooledBufferPtr);  // 供下一次 BeginAsync/Upload 重用

                // Readback copy（AddEnqueueCopyPass 要求在 Execute() 前呼叫）
                auto* Readback = new FRHIGPUBufferReadback(TEXT("CaGpuAsyncReadback"));
                AddEnqueueCopyPass(GB, Readback, Buf, ByteCount);

                GB.Execute();

                // Execute() 後 GPU 命令已提交、readback fence 已建立，此時存指標才安全。
                // memory_order_release 確保 game thread 下一幀 load(acquire) 能看到 Readback 全部初始化。
                AsyncReadbackPtr->store(Readback, std::memory_order_release);
            }
        });
    // ⚠️ 意圖上不呼叫 FlushRenderingCommands()——這是 Phase 5 的核心：GPU 工作與下一幀 game thread 並發執行
    return true;
}

bool FCaGpuSimulator::TryCollectAsync(FWorldCellCb WorldCellCb)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_CaGpu_TryCollect);

    FRHIGPUBufferReadback* Readback = AsyncReadbackInFlight.load(std::memory_order_acquire);
    if (!Readback) return false;

    // IsReady() 可在 game thread 呼叫（只是 fence 查詢，不需要 render thread）。
    // 一般情況下，1 幀後 GPU 已完成，這裡直接 true；極端情況（GPU 很忙）才 false。
    if (!Readback->IsReady()) return false;

    // 清除 atomic，讓 BeginAsync 下次不需要 drop
    AsyncReadbackInFlight.store(nullptr, std::memory_order_release);

    const int32 W         = AsyncW;
    const int32 H         = AsyncH;
    const uint32 ByteCount = static_cast<uint32>(AsyncW * AsyncH * AsyncD) * sizeof(uint32);
    const int32 CX        = AsyncCornerX;
    const int32 CY        = AsyncCornerY;
    const int32 CZ        = AsyncCornerZ;
    TArray<FAsyncCell>* CellsPtr = &AsyncCells;

    // Lock()/Unlock() 要求 render thread；在 render command 裡讀資料，存進 AsyncCells（game thread 後處理）。
    ENQUEUE_RENDER_COMMAND(CaGpuCollectAsync)(
        [Readback, ByteCount, W, H, CX, CY, CZ, CellsPtr](FRHICommandListImmediate&)
        {
            const uint32* Data = static_cast<const uint32*>(Readback->Lock(ByteCount));
            const int32 NumCells = static_cast<int32>(ByteCount / sizeof(uint32));
            CellsPtr->Reset(NumCells / 8);  // 預分配：假設約 12.5% dirty

            for (int32 i = 0; i < NumCells; ++i)
            {
                if (!CaCellPacking::IsDirty(Data[i])) continue;
                const int32 z = i / (H * W);
                const int32 r = i % (H * W);
                const int32 y = r / W;
                const int32 x = r % W;
                CellsPtr->Add({ CX + x, CY + y, CZ + z, Data[i] });
            }

            Readback->Unlock();
            delete Readback;
        });

    // 此 FlushRenderingCommands 只等 Lock/read/Unlock，GPU 工作已在上一幀完成，
    // 所以這個 flush 極快（不再包含 GPU 等待時間）。
    FlushRenderingCommands();

    // AsyncCells 現在由 game thread 獨佔（render command 已結束）
    for (const FAsyncCell& C : AsyncCells)
        WorldCellCb(C.WX, C.WY, C.WZ, C.Packed);
    AsyncCells.Reset();

    return true;
}

void FCaGpuSimulator::Release()
{
    if (PooledBuffer.IsValid())
    {
        TRefCountPtr<FRDGPooledBuffer> LocalPooled = PooledBuffer;
        ENQUEUE_RENDER_COMMAND(CaGpuRelease)([LocalPooled](FRHICommandListImmediate&) { /* TRefCountPtr 釋放於此 */ });
        FlushRenderingCommands();
        PooledBuffer.SafeRelease();
    }

    // Phase 5：清理飛行中的 async readback（若世界在 GPU 工作未完成前被銷毀）
    FRHIGPUBufferReadback* Pending = AsyncReadbackInFlight.exchange(nullptr, std::memory_order_acquire);
    if (Pending)
    {
        ENQUEUE_RENDER_COMMAND(CaGpuReleaseAsync)([Pending](FRHICommandListImmediate&) { delete Pending; });
        FlushRenderingCommands();
    }
    AsyncCells.Empty();

    bAvailable = false;
}
