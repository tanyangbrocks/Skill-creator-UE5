// ══════════════════════════════════════════════════════════════════════
//  M-10 Phase 0 spike 驗證測試——需要真正 RHI，不能用 -nullrhi 跑
//  （對應 docs/plan-m10-gpu-ca.md Phase 0）。
//
//  執行方式（必須是有真正算圖卡裝置的一般 Editor/-game 啟動，不要加 -nullrhi）：
//    UnrealEditor-Cmd.exe "<uproject>" -ExecCmds="Automation RunTests VoxelWorldShaders.M10.GpuSpike; Quit" -unattended -nopause -log
// ══════════════════════════════════════════════════════════════════════

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "M10Spike.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM10GpuSpikeTest,
    "VoxelWorldShaders.M10.GpuSpike",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FM10GpuSpikeTest::RunTest(const FString&)
{
    FString Error;
    const bool bOk = M10Spike::RunSpikeAndVerify(256, Error);
    TestTrue(FString::Printf(TEXT("GPU spike round-trip 正確（256 格）：%s"), *Error), bOk);
    return bOk;
}

#endif
