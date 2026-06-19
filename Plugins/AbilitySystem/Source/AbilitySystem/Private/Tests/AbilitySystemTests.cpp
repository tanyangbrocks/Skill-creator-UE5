#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ExecutionContext.h"
#include "ExecutionLoop.h"
#include "SpellCompiler.h"
#include "SpellRunner.h"
#include "FBlockNodeSaveData.h"

DEFINE_LOG_CATEGORY_STATIC(LogAbilityTest, Log, All);

// ── 工具：快速建立 FInstruction ──────────────────────────────────

template<typename T>
static FInstruction MakeInstr(EOpCode Op, const T& Args)
{
    FInstruction I;
    I.OpCode  = Op;
    I.Payload = FInstancedStruct::Make<T>(Args);
    return I;
}

static FInstruction MakeSetVar(FName Name, float Val, bool bGlobal = false)
{
    FSetVarArgs A;
    A.VarName       = Name;
    A.Value.Val     = Val;
    A.bGlobal       = bGlobal;
    return MakeInstr(EOpCode::SetVar, A);
}

static FInstruction MakeJump(int32 TargetPC)
{
    FJumpArgs A; A.TargetPC = TargetPC;
    return MakeInstr(EOpCode::Jump, A);
}

// ══════════════════════════════════════════════════════════════════
//  T01 — SetVar + 立即完成
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTest_SetVar,
    "AbilitySystem.VM.SetVar",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAbilityTest_SetVar::RunTest(const FString&)
{
    TArray<FInstruction> Code;
    Code.Add(MakeSetVar("x", 42.f));
    Code.Add(MakeSetVar("y", 7.f));

    FExecutionContext Ctx(MoveTemp(Code));
    FExecutionLoop   Loop;
    Loop.ResetTick();
    Loop.Step(Ctx, 0.016f);

    TestTrue(TEXT("State = Completed"), Ctx.State == EExecutionState::Completed);
    TestEqual(TEXT("x == 42"), Ctx.InstanceVars.FindRef("x"), 42.f);
    TestEqual(TEXT("y == 7"),  Ctx.InstanceVars.FindRef("y"),  7.f);
    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T02 — Wait 計時
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTest_Wait,
    "AbilitySystem.VM.Wait",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAbilityTest_Wait::RunTest(const FString&)
{
    FWaitArgs WA; WA.Duration.Val = 0.5f;
    TArray<FInstruction> Code;
    Code.Add(MakeInstr(EOpCode::Wait, WA));
    Code.Add(MakeSetVar("done", 1.f));

    FExecutionContext Ctx(MoveTemp(Code));
    FExecutionLoop   Loop;

    // 第 1 幀：Wait 開始
    Loop.ResetTick();
    Loop.Step(Ctx, 0.016f);
    TestTrue(TEXT("Waiting after first tick"), Ctx.State == EExecutionState::Waiting);
    TestEqual(TEXT("done not set yet"), Ctx.InstanceVars.FindRef("done"), 0.f);

    // 模擬 0.3 秒（尚未到 0.5）
    Loop.ResetTick();
    Loop.Step(Ctx, 0.3f);
    TestTrue(TEXT("Still waiting at 0.316s"), Ctx.State == EExecutionState::Waiting);

    // 超過閾值（剩餘 ≈ 0.184，再 step 0.2）
    Loop.ResetTick();
    Loop.Step(Ctx, 0.2f);
    TestTrue(TEXT("Completed after timer"), Ctx.State == EExecutionState::Completed);
    TestEqual(TEXT("done == 1"), Ctx.InstanceVars.FindRef("done"), 1.f);
    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T03 — RepeatN 迴圈
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTest_RepeatN,
    "AbilitySystem.VM.RepeatN",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAbilityTest_RepeatN::RunTest(const FString&)
{
    // RepeatPush(3) → SetVar("x", x+1) [用 Var] → RepeatStep(1)
    // 因為 SetVar 只能設字面值，改成 accumulate via 3 separate SetVar：
    // 簡易測試：迴圈3次，每次把 counter 加字面 1（用 Var ref 累加）
    // 這裡直接驗證迴圈產生正確的指令序列行為

    FRepeatPushArgs PushA; PushA.Count.Val = 3.f;
    FRepeatStepArgs StepA; StepA.LoopStartPC = 1;  // 迴圈體 = index 1

    // counter 透過 Var ref 累加：SetVar("cnt", {Var="cnt"}) + 1 不支援
    // 改用：每次 invoke 對 "hits" 寫 1，最終只取最後一次（驗證迴圈確實執行）
    // 真正的累加在 SpellCompiler + 遊戲層；這裡只測試 RepeatN 結構本身
    TArray<FInstruction> Code;
    Code.Add(MakeInstr(EOpCode::RepeatPush, PushA));  // PC=0
    Code.Add(MakeSetVar("iter", 1.f));                 // PC=1（迴圈體）
    Code.Add(MakeInstr(EOpCode::RepeatStep, StepA));   // PC=2

    FExecutionContext Ctx(MoveTemp(Code));
    FExecutionLoop   Loop;
    Loop.ResetTick();
    Loop.Step(Ctx, 0.f);

    TestTrue(TEXT("RepeatN completed"), Ctx.State == EExecutionState::Completed);
    TestEqual(TEXT("iter set in body"), Ctx.InstanceVars.FindRef("iter"), 1.f);
    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T04 — If/else 分支（JumpIfFalse + Jump）
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTest_IfElse,
    "AbilitySystem.VM.IfElse",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAbilityTest_IfElse::RunTest(const FString&)
{
    // if (1 > 2) { branch="then" } else { branch="else" }
    // Expected: condition false → jump to else
    FConditionArgs Cond;
    Cond.Type      = EConditionType::Compare;
    Cond.Left.Val  = 1.f;
    Cond.Right.Val = 2.f;
    Cond.Op        = ">";

    FJumpIfArgs JifA; JifA.Cond = Cond; JifA.TargetPC = 3;  // → else (PC=3)
    FJumpArgs   JmpA; JmpA.TargetPC = 4;                      // → end (PC=4)

    TArray<FInstruction> Code;
    Code.Add(MakeInstr(EOpCode::JumpIfFalse, JifA));   // PC=0
    Code.Add(MakeSetVar("branch", 10.f));               // PC=1 (then: 10)
    Code.Add(MakeInstr(EOpCode::Jump, JmpA));           // PC=2
    Code.Add(MakeSetVar("branch", 20.f));               // PC=3 (else: 20)
    // PC=4 → end

    FExecutionContext Ctx(MoveTemp(Code));
    FExecutionLoop   Loop;
    Loop.ResetTick();
    Loop.Step(Ctx, 0.f);

    TestTrue(TEXT("Completed"), Ctx.State == EExecutionState::Completed);
    TestEqual(TEXT("else branch taken (branch==20)"), Ctx.InstanceVars.FindRef("branch"), 20.f);

    // now test true branch: 5 > 2
    Cond.Left.Val = 5.f;
    JifA.Cond = Cond;
    TArray<FInstruction> Code2;
    Code2.Add(MakeInstr(EOpCode::JumpIfFalse, JifA));
    Code2.Add(MakeSetVar("branch", 10.f));
    Code2.Add(MakeInstr(EOpCode::Jump, JmpA));
    Code2.Add(MakeSetVar("branch", 20.f));

    FExecutionContext Ctx2(MoveTemp(Code2));
    FExecutionLoop   Loop2;
    Loop2.ResetTick();
    Loop2.Step(Ctx2, 0.f);

    TestEqual(TEXT("then branch taken (branch==10)"), Ctx2.InstanceVars.FindRef("branch"), 10.f);
    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T05 — StoreCompare（Compare 積木）
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTest_StoreCompare,
    "AbilitySystem.VM.StoreCompare",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAbilityTest_StoreCompare::RunTest(const FString&)
{
    FStoreCompareArgs CA;
    CA.Left.Val  = 5.f;
    CA.Right.Val = 3.f;
    CA.Op        = ">";
    CA.ResultVar = "result";

    TArray<FInstruction> Code;
    Code.Add(MakeInstr(EOpCode::StoreCompare, CA));

    FExecutionContext Ctx(MoveTemp(Code));
    FExecutionLoop   Loop;
    Loop.ResetTick();
    Loop.Step(Ctx, 0.f);

    TestEqual(TEXT("5 > 3 → result == 1"), Ctx.InstanceVars.FindRef("result"), 1.f);

    // 反向
    CA.Left.Val = 1.f;
    TArray<FInstruction> Code2;
    Code2.Add(MakeInstr(EOpCode::StoreCompare, CA));
    FExecutionContext Ctx2(MoveTemp(Code2));
    FExecutionLoop   Loop2;
    Loop2.ResetTick();
    Loop2.Step(Ctx2, 0.f);
    TestEqual(TEXT("1 > 3 → result == 0"), Ctx2.InstanceVars.FindRef("result"), 0.f);

    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T06 — 3D VecMake + VecAdd
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTest_VecMakeAdd,
    "AbilitySystem.VM.VecMakeAdd",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAbilityTest_VecMakeAdd::RunTest(const FString&)
{
    FVecMakeArgs MA;
    MA.ResultName = "a";
    MA.X.Val = 1.f; MA.Y.Val = 2.f; MA.Z.Val = 3.f;

    FVecMakeArgs MB;
    MB.ResultName = "b";
    MB.X.Val = 4.f; MB.Y.Val = 5.f; MB.Z.Val = 6.f;

    FVecBinopArgs AddA;
    AddA.VecA = "a"; AddA.VecB = "b"; AddA.Result = "c";

    TArray<FInstruction> Code;
    Code.Add(MakeInstr(EOpCode::VecMake, MA));
    Code.Add(MakeInstr(EOpCode::VecMake, MB));
    Code.Add(MakeInstr(EOpCode::VecAdd,  AddA));

    FExecutionContext Ctx(MoveTemp(Code));
    FExecutionLoop   Loop;
    Loop.ResetTick();
    Loop.Step(Ctx, 0.f);

    TestEqual(TEXT("c.x == 5"), Ctx.InstanceVars.FindRef("c.x"), 5.f);
    TestEqual(TEXT("c.y == 7"), Ctx.InstanceVars.FindRef("c.y"), 7.f);
    TestEqual(TEXT("c.z == 9"), Ctx.InstanceVars.FindRef("c.z"), 9.f);
    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T07 — VecNorm 正規化
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTest_VecNorm,
    "AbilitySystem.VM.VecNorm",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAbilityTest_VecNorm::RunTest(const FString&)
{
    // VecMake(v, 0, 3, 4) → VecNorm(v, n) → |n| == 1
    FVecMakeArgs MA; MA.ResultName = "v"; MA.X.Val = 0.f; MA.Y.Val = 3.f; MA.Z.Val = 4.f;
    FVecUnopArgs NA; NA.Vec = "v"; NA.Result = "n";

    FVecUnopArgs LenA; LenA.Vec = "n"; LenA.Result = "len";

    TArray<FInstruction> Code;
    Code.Add(MakeInstr(EOpCode::VecMake,   MA));
    Code.Add(MakeInstr(EOpCode::VecNorm,   NA));
    Code.Add(MakeInstr(EOpCode::VecLength, LenA));

    FExecutionContext Ctx(MoveTemp(Code));
    FExecutionLoop   Loop;
    Loop.ResetTick();
    Loop.Step(Ctx, 0.f);

    float Len = Ctx.InstanceVars.FindRef("len");
    TestTrue(TEXT("|normalized| ≈ 1"), FMath::IsNearlyEqual(Len, 1.f, 0.0001f));
    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T08 — SpellRunner Tick（完整執行）
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTest_SpellRunner,
    "AbilitySystem.VM.SpellRunner",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAbilityTest_SpellRunner::RunTest(const FString&)
{
    TArray<FInstruction> Code;
    Code.Add(MakeSetVar("result", 99.f));

    auto Ctx = MakeUnique<FExecutionContext>(MoveTemp(Code));
    float* ResultPtr = nullptr;

    FSpellRunner Runner;
    Runner.Submit(MoveTemp(Ctx));
    Runner.Tick(0.016f);

    TestEqual(TEXT("ActiveCount == 0 after completion"), Runner.GetActiveCount(), 0);
    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T09 — List 操作
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTest_List,
    "AbilitySystem.VM.List",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAbilityTest_List::RunTest(const FString&)
{
    FListArgs CreateA; CreateA.ListName = "myList";
    FListArgs AppA1;   AppA1.ListName = "myList"; AppA1.Value.Val = 10.f;
    FListArgs AppA2;   AppA2.ListName = "myList"; AppA2.Value.Val = 20.f;
    FListArgs AppA3;   AppA3.ListName = "myList"; AppA3.Value.Val = 30.f;
    FListArgs LenA;    LenA.ListName  = "myList"; LenA.ResultVar  = "len";
    FListArgs GetA;    GetA.ListName  = "myList"; GetA.Index.Val  = 2.f; GetA.ResultVar = "mid";
    FListArgs PopA;    PopA.ListName  = "myList"; PopA.ResultVar  = "popped";

    TArray<FInstruction> Code;
    Code.Add(MakeInstr(EOpCode::ListCreate, CreateA));
    Code.Add(MakeInstr(EOpCode::ListAppend, AppA1));
    Code.Add(MakeInstr(EOpCode::ListAppend, AppA2));
    Code.Add(MakeInstr(EOpCode::ListAppend, AppA3));
    Code.Add(MakeInstr(EOpCode::ListLength, LenA));
    Code.Add(MakeInstr(EOpCode::ListGet,    GetA));   // index 2 → value 20
    Code.Add(MakeInstr(EOpCode::ListPop,    PopA));   // pops 30

    FExecutionContext Ctx(MoveTemp(Code));
    FExecutionLoop   Loop;
    Loop.ResetTick();
    Loop.Step(Ctx, 0.f);

    TestEqual(TEXT("len == 3"), Ctx.InstanceVars.FindRef("len"),    3.f);
    TestEqual(TEXT("mid == 20"), Ctx.InstanceVars.FindRef("mid"),   20.f);
    TestEqual(TEXT("popped == 30"), Ctx.InstanceVars.FindRef("popped"), 30.f);
    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T10 — SpellCompiler：If 積木編譯
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTest_CompilerIf,
    "AbilitySystem.VM.CompilerIf",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAbilityTest_CompilerIf::RunTest(const FString&)
{
    // Compile: if (3 > 1) { result=1 } else { result=0 }
    FConditionArgs Cond;
    Cond.Type      = EConditionType::Compare;
    Cond.Left.Val  = 3.f;
    Cond.Right.Val = 1.f;
    Cond.Op        = ">";

    // ThenBranch: SetVar("result", 1)
    TUniquePtr<FBlockNode> ThenBlock = MakeUnique<FBlockNode>();
    ThenBlock->Type = EBlockType::SetVar;
    {
        FSetVarArgs A; A.VarName = "result"; A.Value.Val = 1.f;
        ThenBlock->Params.Add("args", FInstancedStruct::Make<FSetVarArgs>(A));
    }

    // ElseBranch: SetVar("result", 0)
    TUniquePtr<FBlockNode> ElseBlock = MakeUnique<FBlockNode>();
    ElseBlock->Type = EBlockType::SetVar;
    {
        FSetVarArgs A; A.VarName = "result"; A.Value.Val = 0.f;
        ElseBlock->Params.Add("args", FInstancedStruct::Make<FSetVarArgs>(A));
    }

    TUniquePtr<FBlockNode> IfBlock = MakeUnique<FBlockNode>();
    IfBlock->Type = EBlockType::If;
    IfBlock->Params.Add("cond", FInstancedStruct::Make<FConditionArgs>(Cond));
    IfBlock->ThenBranch.Add(MoveTemp(ThenBlock));
    IfBlock->ElseBranch.Add(MoveTemp(ElseBlock));

    TArray<TUniquePtr<FBlockNode>> Root;
    Root.Add(MoveTemp(IfBlock));

    TArray<FInstruction> Code = FSpellCompiler::Compile(Root);
    TestTrue(TEXT("Code generated"), Code.Num() >= 3);

    FExecutionContext Ctx(MoveTemp(Code));
    FExecutionLoop   Loop;
    Loop.ResetTick();
    Loop.Step(Ctx, 0.f);

    TestTrue(TEXT("Completed"), Ctx.State == EExecutionState::Completed);
    TestEqual(TEXT("3>1 → then → result==1"), Ctx.InstanceVars.FindRef("result"), 1.f);
    return true;
}

// ══════════════════════════════════════════════════════════════════
//  T-Save — FBlockTreeSaveData 攤平/還原往返（積木編輯器存檔持久化用）
//  覆蓋：If + ThenBranch + ElseBranch 巢狀結構、Params（FInstancedStruct）
//  正確攤平成 index pool 並還原，編譯後執行結果與還原前一致。
// ══════════════════════════════════════════════════════════════════
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityTest_BlockTreeSaveRoundTrip,
    "AbilitySystem.Save.BlockTreeRoundTrip",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAbilityTest_BlockTreeSaveRoundTrip::RunTest(const FString&)
{
    // 建一棵跟上面 If/Else 測試相同的樹：3 > 1 → then(result=1) / else(result=0)
    FConditionArgs Cond;
    Cond.Type  = EConditionType::Compare;
    Cond.Left  = FNumRef::Lit(3.f);
    Cond.Right = FNumRef::Lit(1.f);

    TUniquePtr<FBlockNode> ThenBlock = MakeUnique<FBlockNode>();
    ThenBlock->Type = EBlockType::SetVar;
    {
        FSetVarArgs A; A.VarName = "result"; A.Value.Val = 1.f;
        ThenBlock->Params.Add("args", FInstancedStruct::Make<FSetVarArgs>(A));
    }

    TUniquePtr<FBlockNode> ElseBlock = MakeUnique<FBlockNode>();
    ElseBlock->Type = EBlockType::SetVar;
    {
        FSetVarArgs A; A.VarName = "result"; A.Value.Val = 0.f;
        ElseBlock->Params.Add("args", FInstancedStruct::Make<FSetVarArgs>(A));
    }

    TUniquePtr<FBlockNode> IfBlock = MakeUnique<FBlockNode>();
    IfBlock->Type = EBlockType::If;
    IfBlock->Params.Add("cond", FInstancedStruct::Make<FConditionArgs>(Cond));
    IfBlock->ThenBranch.Add(MoveTemp(ThenBlock));
    IfBlock->ElseBranch.Add(MoveTemp(ElseBlock));

    TArray<TUniquePtr<FBlockNode>> Original;
    Original.Add(MoveTemp(IfBlock));

    // 攤平 → 還原
    FBlockTreeSaveData Tree = FBlockTreeSaveData::FromBlocks(Original);
    TestEqual(TEXT("攤平後 Pool 含 3 個節點（If+Then+Else）"), Tree.Pool.Num(), 3);
    TestEqual(TEXT("Roots 只有 1 個根節點"), Tree.Roots.Num(), 1);

    TArray<TUniquePtr<FBlockNode>> Restored = FBlockTreeSaveData::ToBlocks(Tree);
    TestEqual(TEXT("還原後根節點數一致"), Restored.Num(), 1);
    TestTrue(TEXT("根節點型別為 If"), Restored[0]->Type == EBlockType::If);
    TestEqual(TEXT("ThenBranch 還原 1 個節點"), Restored[0]->ThenBranch.Num(), 1);
    TestEqual(TEXT("ElseBranch 還原 1 個節點"), Restored[0]->ElseBranch.Num(), 1);

    // 還原後的樹編譯+執行，結果應與原始樹一致（驗證 Params/FInstancedStruct 沒有失真）
    TArray<FInstruction> Code = FSpellCompiler::Compile(Restored);
    FExecutionContext Ctx(MoveTemp(Code));
    FExecutionLoop   Loop;
    Loop.ResetTick();
    Loop.Step(Ctx, 0.f);

    TestTrue(TEXT("還原後編譯執行 Completed"), Ctx.State == EExecutionState::Completed);
    TestEqual(TEXT("還原後 3>1 → then → result==1"), Ctx.InstanceVars.FindRef("result"), 1.f);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
