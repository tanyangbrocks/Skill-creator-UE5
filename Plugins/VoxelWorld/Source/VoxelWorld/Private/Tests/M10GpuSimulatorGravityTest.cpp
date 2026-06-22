// ══════════════════════════════════════════════════════════════════════
//  M-10 Phase 2：FCaGpuSimulator 公開 API 重力驗證測試——需要真正 RHI，不能用 -nullrhi 跑
//  （對應 docs/plan-m10-gpu-ca.md Phase 2）。
//
//  執行方式：
//    UnrealEditor-Cmd.exe "<uproject>" -ExecCmds="Automation RunTests VoxelWorld.M10.GpuSimulatorGravity; Quit" -unattended -nopause -log
// ══════════════════════════════════════════════════════════════════════

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "M10GpuSimulatorTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM10GpuSimulatorGravityTest,
    "VoxelWorld.M10.GpuSimulatorGravity",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FM10GpuSimulatorGravityTest::RunTest(const FString&)
{
    FString Error;
    const bool bOk = M10GpuSimulatorTest::RunGravityTestAndVerify(Error);
    TestTrue(FString::Printf(TEXT("FCaGpuSimulator Upload/Simulate/Download 公開 API 正確：%s"), *Error), bOk);
    return bOk;
}

#endif
