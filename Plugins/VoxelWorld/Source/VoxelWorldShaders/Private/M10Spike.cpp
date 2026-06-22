#include "M10Spike.h"
#include "CaSpikeShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "GlobalShader.h"
#include "RenderingThread.h"

bool M10Spike::RunSpikeAndVerify(int32 Count, FString& OutError)
{
    if (!GIsRHIInitialized)
    {
        OutError = TEXT("RHI 未初始化（是否用 -nullrhi 啟動？這個測試需要真正的 RHI）");
        return false;
    }

    // FRHIGPUBufferReadback 不能在 game thread 建構——其建構子內部呼叫
    // RHICreateGPUFence()，這個 API 要求一定要在 render thread 上呼叫（之前在 game thread
    // 先 new 出來，跑出 Assertion failed: IsInRenderingThread()）。改成在 render thread
    // lambda 裡面建構，透過參照寫回 game thread 這個變數；FlushRenderingCommands() 之後
    // 保證 render thread 已經把它寫好，之後只有 game thread 存取，沒有真正跨執行緒併發。
    FRHIGPUBufferReadback* Readback = nullptr;
    const uint32 ByteCount = (uint32)Count * sizeof(uint32);

    ENQUEUE_RENDER_COMMAND(M10Spike)(
        [Count, ByteCount, &Readback](FRHICommandListImmediate& RHICmdList)
        {
            Readback = new FRHIGPUBufferReadback(TEXT("M10SpikeReadback"));

            FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("M10Spike"));

            FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(
                FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), Count), TEXT("M10SpikeBuffer"));

            FCaSpikeCS::FParameters* Params = GraphBuilder.AllocParameters<FCaSpikeCS::FParameters>();
            Params->OutputBuffer = GraphBuilder.CreateUAV(Buffer);
            Params->Count        = (uint32)Count;

            TShaderMapRef<FCaSpikeCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
            FComputeShaderUtils::AddPass(
                GraphBuilder, RDG_EVENT_NAME("M10SpikeDispatch"), ComputeShader, Params,
                FIntVector(FMath::DivideAndRoundUp(Count, 64), 1, 1));

            AddEnqueueCopyPass(GraphBuilder, Readback, Buffer, ByteCount);

            GraphBuilder.Execute();
        });

    // 確保上面那個 render command 真的被 render thread 執行了（把 dispatch 送進 GPU）。
    // 注意：這只保證「送出」，不保證 GPU 已經算完——GPU 完成與否要靠下面的 Readback->IsReady() fence。
    FlushRenderingCommands();

    // Poll() 本身不要求 render thread（跟 Wait() 不同，見 RHIResources.h FRHIGPUFence 文件
    // 註解），所以這個忙等迴圈留在 game thread 沒問題。
    const double StartTime = FPlatformTime::Seconds();
    while (!Readback->IsReady())
    {
        FPlatformProcess::Sleep(0.001f);
        if (FPlatformTime::Seconds() - StartTime > 5.0)
        {
            OutError = TEXT("Readback 逾時（5 秒），GPU 可能沒有完成或 fence 沒被觸發");
            ENQUEUE_RENDER_COMMAND(M10SpikeCleanup)([Readback](FRHICommandListImmediate&) { delete Readback; });
            FlushRenderingCommands();
            return false;
        }
    }

    // Lock()/Unlock() 內部會呼叫 LockStagingBuffer_RenderThread()，函式名稱本身就講明白
    // 要求 render thread（RHICommandList.cpp:2185 check(IsInRenderingThread())）——這正是
    // 上一輪撞到的 assert。跟建構 FRHIGPUBufferReadback 同一個道理，搬進 render command。
    bool bOk = true;
    FString LocalError;
    ENQUEUE_RENDER_COMMAND(M10SpikeReadbackVerify)(
        [Count, ByteCount, Readback, &bOk, &LocalError](FRHICommandListImmediate& RHICmdList)
        {
            const uint32* Data = static_cast<const uint32*>(Readback->Lock(ByteCount));
            for (int32 i = 0; i < Count; ++i)
            {
                if (Data[i] != static_cast<uint32>(i * 2))
                {
                    LocalError = FString::Printf(TEXT("第 %d 格錯誤：預期 %u，實際 %u"), i, (uint32)(i * 2), Data[i]);
                    bOk = false;
                    break;
                }
            }
            Readback->Unlock();
            delete Readback;
        });

    FlushRenderingCommands();

    if (!bOk) OutError = LocalError;
    return bOk;
}
