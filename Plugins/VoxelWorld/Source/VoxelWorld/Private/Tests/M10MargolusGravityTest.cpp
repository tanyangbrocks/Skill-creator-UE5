// ══════════════════════════════════════════════════════════════════════
//  M-10 Phase 1：Margolus CA shader 重力驗證測試——需要真正 RHI，不能用 -nullrhi 跑
//  （對應 docs/plan-m10-gpu-ca.md Phase 1）。
//
//  執行方式：
//    UnrealEditor-Cmd.exe "<uproject>" -ExecCmds="Automation RunTests VoxelWorld.M10.MargolusGravity; Quit" -unattended -nopause -log
// ══════════════════════════════════════════════════════════════════════

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "M10MargolusTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM10MargolusGravityTest,
    "VoxelWorld.M10.MargolusGravity",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FM10MargolusGravityTest::RunTest(const FString&)
{
    FString Error;
    const bool bOk = M10MargolusTest::RunGravityTestAndVerify(Error);
    TestTrue(FString::Printf(TEXT("Margolus CA 重力邏輯正確（Sand 落到預期位置）：%s"), *Error), bOk);
    return bOk;
}

#endif
