#include "M10MargolusTest.h"
#include "CaCellPacking.h"
#include "CaMargolusShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "GlobalShader.h"
#include "RenderingThread.h"

// 對應 Godot CaGpuSimulator.cs:204-207 的 dispatch group 數計算：
// 每個 invocation 處理一個 2x2x2 block（2 格/軸），每個 thread group 是 4x4x4 個
// invocation（numthreads(4,4,4)，CaMargolus.usf），所以每個 thread group 覆蓋 8 格/軸。
// groups = ceil(ceil(Size/2)/4)。
// 名稱加 _Phase1Test 後綴：CaGpuSimulator.cpp 也有同一個公式的 static 函式，這個專案用
// Unity Build（整個模組的 .cpp 合併成一個翻譯單元編譯），「同檔案內 static」這個慣常防護
// 在 Unity Build 下失效，撞名會變成編譯錯誤（已撞過一次），用後綴避免再撞。
static int32 ComputeMargolusGroups_Phase1Test(int32 Size)
{
    const int32 HalfSize = (Size + 1) / 2; // ceil(Size/2)
    return FMath::Max((HalfSize + 3) / 4, 1); // ceil(HalfSize/4)，至少 1 個 group
}

bool M10MargolusTest::RunGravityTestAndVerify(FString& OutError)
{
    if (!GIsRHIInitialized)
    {
        OutError = TEXT("RHI 未初始化（是否用 -nullrhi 啟動？這個測試需要真正的 RHI）");
        return false;
    }

    const int32 W = 4, H = 4, D = 4;
    const int32 NumCells = W * H * D;

    auto Idx = [W, H](int32 x, int32 y, int32 z) { return z * H * W + y * W + x; };

    TArray<uint32> InitialData;
    InitialData.SetNumZeroed(NumCells); // 全部 Air（packed=0）
    InitialData[Idx(0, 0, 0)] = CaCellPacking::Pack(static_cast<uint8>(EMaterialType::Sand));

    // 同 M10Spike 的教訓：FRHIGPUBufferReadback 要在 render thread 建構
    // （RHICreateGPUFence 要求），不能在 game thread new。
    FRHIGPUBufferReadback* Readback = nullptr;
    const uint32 ByteCount = (uint32)NumCells * sizeof(uint32);
    const int32 Groups = ComputeMargolusGroups_Phase1Test(W); // W=H=D=4，三軸相同

    ENQUEUE_RENDER_COMMAND(M10Margolus)(
        [NumCells, ByteCount, InitialData, Groups, &Readback](FRHICommandListImmediate& RHICmdList)
        {
            Readback = new FRHIGPUBufferReadback(TEXT("M10MargolusReadback"));

            FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("M10Margolus"));

            FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(
                FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumCells), TEXT("M10MargolusBuffer"));

            GraphBuilder.QueueBufferUpload(Buffer, InitialData.GetData(), InitialData.Num() * sizeof(uint32));

            TShaderMapRef<FCaMargolusCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

            // 對應 Godot CaGpuSimulator.cs:217-230：每次 Simulate 對 Phase 0/1 各 dispatch 一次。
            for (int32 Phase = 0; Phase < 2; ++Phase)
            {
                FCaMargolusCS::FParameters* Params = GraphBuilder.AllocParameters<FCaMargolusCS::FParameters>();
                Params->Cells  = GraphBuilder.CreateUAV(Buffer);
                Params->W      = W;
                Params->H      = H;
                Params->D      = D;
                Params->Phase  = Phase;
                Params->Rng    = 12345u;
                Params->ZFrom  = 0;
                Params->ZCount = D;

                FComputeShaderUtils::AddPass(
                    GraphBuilder, RDG_EVENT_NAME("M10MargolusDispatch"), ComputeShader, Params,
                    FIntVector(Groups, Groups, Groups));
            }

            AddEnqueueCopyPass(GraphBuilder, Readback, Buffer, ByteCount);

            GraphBuilder.Execute();
        });

    FlushRenderingCommands();

    const double StartTime = FPlatformTime::Seconds();
    while (!Readback->IsReady())
    {
        FPlatformProcess::Sleep(0.001f);
        if (FPlatformTime::Seconds() - StartTime > 5.0)
        {
            OutError = TEXT("Readback 逾時（5 秒），GPU 可能沒有完成或 fence 沒被觸發");
            ENQUEUE_RENDER_COMMAND(M10MargolusCleanup)([Readback](FRHICommandListImmediate&) { delete Readback; });
            FlushRenderingCommands();
            return false;
        }
    }

    bool bOk = true;
    FString LocalError;
    ENQUEUE_RENDER_COMMAND(M10MargolusVerify)(
        [ByteCount, Readback, &bOk, &LocalError, W, H](FRHICommandListImmediate&)
        {
            const uint32* Data = static_cast<const uint32*>(Readback->Lock(ByteCount));

            auto LocalIdx = [W, H](int32 x, int32 y, int32 z) { return z * H * W + y * W + x; };

            const uint32 OriginCell = Data[LocalIdx(0, 0, 0)];
            const uint32 FallenCell = Data[LocalIdx(0, 1, 0)];

            if (CaCellPacking::UnpackMaterialID(OriginCell) != 0)
            {
                LocalError = FString::Printf(TEXT("原本的格子 (0,0,0) 應該變空，但 MaterialID=%d"),
                    CaCellPacking::UnpackMaterialID(OriginCell));
                bOk = false;
            }
            else if (CaCellPacking::UnpackMaterialID(FallenCell) != static_cast<uint8>(EMaterialType::Sand))
            {
                LocalError = FString::Printf(TEXT("Sand 沒有落到 (0,1,0)，那格的 MaterialID=%d"),
                    CaCellPacking::UnpackMaterialID(FallenCell));
                bOk = false;
            }

            Readback->Unlock();
            delete Readback;
        });

    FlushRenderingCommands();

    if (!bOk) OutError = LocalError;
    return bOk;
}
