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

void FCaGpuSimulator::Release()
{
    if (PooledBuffer.IsValid())
    {
        TRefCountPtr<FRDGPooledBuffer> LocalPooled = PooledBuffer;
        ENQUEUE_RENDER_COMMAND(CaGpuRelease)([LocalPooled](FRHICommandListImmediate&) { /* TRefCountPtr 釋放於此 */ });
        FlushRenderingCommands();
        PooledBuffer.SafeRelease();
    }
    bAvailable = false;
}
