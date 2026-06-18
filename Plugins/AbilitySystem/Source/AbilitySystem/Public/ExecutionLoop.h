#pragma once
#include "CoreMinimal.h"
#include "ExecutionContext.h"

// ══════════════════════════════════════════════════════════════════
//  FExecutionLoop — 單步 VM 執行器
//
//  每幀由 SpellRunner 呼叫一次 Step()；Wait 計時、條件等待、邊緣觸發
//  都在 Step 頂部處理，完成後才進入 Execute 分派迴圈。
// ══════════════════════════════════════════════════════════════════
class FExecutionLoop
{
public:
    // FExecutionLoop 內部預算（注意：FSafetyGuard 有同名但不同層次的常數；此處為單步 Execute 範圍限制）
    static constexpr int32 MaxOpcodeDispatchPerStep = 2000;  // 單步 Execute() 最大 opcode 分派數
    static constexpr int32 MaxWhileLoopIterations   = 1000;  // RepeatWhile 最大迭代數（同 FSafetyGuard::MaxWhileIterations 但獨立計數）

    void ResetTick() { ExecutionsThisTick = 0; }

    // 執行一步；回傳 false 表示本幀預算耗盡
    bool Step(FExecutionContext& Ctx, float DeltaTime);

private:
    int32 ExecutionsThisTick = 0;

    void Execute(const FInstruction& Instr, FExecutionContext& Ctx);

    bool EvalCondition(const FConditionArgs& Cond, const FExecutionContext& Ctx) const;
    bool EvalCompare(const FConditionArgs& Cond, const FExecutionContext& Ctx) const;

    static float   ResolveNum(const FNumRef& Ref, const FExecutionContext& Ctx);

    // VM Debug 追蹤：格式化單條指令為可讀字串（對應 Godot FormatTraceParams）
    // 輸出格式：PC=N | OpCode | PayloadType [key args…]
    static FString FormatTraceParams(int32 PC, const FInstruction& Instr);

    static FName  VecKey(FName VecName, TCHAR Comp);
    static void   GetVec(const TMap<FName, float>& Vars, FName Name,
                         float& OutX, float& OutY, float& OutZ);
    static void   SetVec(TMap<FName, float>& Vars, FName Name,
                         float X, float Y, float Z);
    static void   SetEntityVars(FExecutionContext& Ctx, const FEntityInfo& E);
};
