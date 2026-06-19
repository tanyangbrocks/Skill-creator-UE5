#pragma once
#include "CoreMinimal.h"
#include "Instruction.h"
#include "FBlockNode.h"

// ══════════════════════════════════════════════════════════════════
//  FSpellCompiler — 將 FBlockNode AST 編譯為 FInstruction 陣列
//
//  FBlockNode.Params 慣例（SpellCompiler 讀取，M-9 編輯器寫入）：
//    "cond"  = FConditionArgs              ← 條件積木（If / RepeatWhile / Edge*）
//    "args"  = FXxxArgs（對應指令的 Payload 型別）← 非條件積木
//
//  測試時手動填入：
//    Block.Params.Add("args", FInstancedStruct::Make<FSetVarArgs>(A));
// ══════════════════════════════════════════════════════════════════
class ABILITYSYSTEM_API FSpellCompiler
{
public:
    // TotemSlotMap: totemId → slotRef（供 BlockType::Totem 積木轉換）
    static TArray<FInstruction> Compile(
        const TArray<TUniquePtr<FBlockNode>>& Blocks,
        const TMap<FName, FName>& TotemSlotMap = TMap<FName, FName>());

private:
    static void EmitList(const TArray<TUniquePtr<FBlockNode>>& Blocks,
                         TArray<FInstruction>& Code,
                         const TMap<FName, FName>& Tsm);

    static void EmitBlock(const FBlockNode& Block,
                          TArray<FInstruction>& Code,
                          const TMap<FName, FName>& Tsm);

    // 從 Block.Params["cond"] 讀取 FConditionArgs
    static FConditionArgs ReadCond(const FBlockNode& Block);

    // 從 Block.Params["args"] 讀取指定型別的 Args struct
    template<typename T>
    static bool ReadArgs(const FBlockNode& Block, T& OutArgs)
    {
        const FInstancedStruct* IS = Block.Params.Find("args");
        if (!IS) return false;
        if (const T* A = IS->GetPtr<T>()) { OutArgs = *A; return true; }
        return false;
    }

    // 建立 FInstruction（強型別 Payload）
    template<typename T>
    static FInstruction MakeI(EOpCode Op, const T& Args)
    {
        FInstruction I;
        I.OpCode  = Op;
        I.Payload = FInstancedStruct::Make<T>(Args);
        return I;
    }

    static FInstruction MakeJump(int32 TargetPC);

    // 回填跳轉目標（在陣列擴展後仍安全，因為用索引不用指標）
    static void PatchJump  (TArray<FInstruction>& Code, int32 Idx, int32 TargetPC);
    static void PatchJumpIf(TArray<FInstruction>& Code, int32 Idx, int32 TargetPC);
    static void PatchWhile (TArray<FInstruction>& Code, int32 Idx, int32 LoopEndPC);
    static void PatchEdge  (TArray<FInstruction>& Code, int32 Idx, int32 TargetPC);
    static void PatchCounter(TArray<FInstruction>& Code, int32 Idx, int32 TargetPC);
    static void PatchForEach(TArray<FInstruction>& Code, int32 Idx, int32 LoopEndPC);
};
