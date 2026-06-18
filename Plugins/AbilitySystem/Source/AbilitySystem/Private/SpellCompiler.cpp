#include "SpellCompiler.h"
#include "BlockType.h"

// ── 靜態輔助 ─────────────────────────────────────────────────────

FConditionArgs FSpellCompiler::ReadCond(const FBlockNode& Block)
{
    const FInstancedStruct* IS = Block.Params.Find("cond");
    if (IS)
        if (const FConditionArgs* A = IS->GetPtr<FConditionArgs>()) return *A;
    return FConditionArgs{};
}

FInstruction FSpellCompiler::MakeJump(int32 TargetPC)
{
    FJumpArgs A; A.TargetPC = TargetPC;
    FInstruction I; I.OpCode = EOpCode::Jump; I.Payload = FInstancedStruct::Make<FJumpArgs>(A);
    return I;
}

void FSpellCompiler::PatchJump(TArray<FInstruction>& Code, int32 Idx, int32 TargetPC)
{
    if (FJumpArgs* A = Code[Idx].Payload.GetMutablePtr<FJumpArgs>()) A->TargetPC = TargetPC;
}

void FSpellCompiler::PatchJumpIf(TArray<FInstruction>& Code, int32 Idx, int32 TargetPC)
{
    if (FJumpIfArgs* A = Code[Idx].Payload.GetMutablePtr<FJumpIfArgs>()) A->TargetPC = TargetPC;
}

void FSpellCompiler::PatchWhile(TArray<FInstruction>& Code, int32 Idx, int32 LoopEndPC)
{
    if (FWhileArgs* A = Code[Idx].Payload.GetMutablePtr<FWhileArgs>()) A->LoopEndPC = LoopEndPC;
}

void FSpellCompiler::PatchEdge(TArray<FInstruction>& Code, int32 Idx, int32 TargetPC)
{
    if (FEdgeArgs* A = Code[Idx].Payload.GetMutablePtr<FEdgeArgs>()) A->TargetPC = TargetPC;
}

void FSpellCompiler::PatchCounter(TArray<FInstruction>& Code, int32 Idx, int32 TargetPC)
{
    if (FTaskCounterArgs* A = Code[Idx].Payload.GetMutablePtr<FTaskCounterArgs>())
        A->TargetPC = TargetPC;
}

void FSpellCompiler::PatchForEach(TArray<FInstruction>& Code, int32 Idx, int32 LoopEndPC)
{
    if (FQueryArgs* A = Code[Idx].Payload.GetMutablePtr<FQueryArgs>()) A->LoopEndPC = LoopEndPC;
}

// ── 公開入口 ─────────────────────────────────────────────────────

TArray<FInstruction> FSpellCompiler::Compile(
    const TArray<TUniquePtr<FBlockNode>>& Blocks,
    const TMap<FName, FName>& TotemSlotMap)
{
    TArray<FInstruction> Code;
    EmitList(Blocks, Code, TotemSlotMap);
    return Code;
}

void FSpellCompiler::EmitList(const TArray<TUniquePtr<FBlockNode>>& Blocks,
                               TArray<FInstruction>& Code,
                               const TMap<FName, FName>& Tsm)
{
    for (const TUniquePtr<FBlockNode>& B : Blocks)
        if (B) EmitBlock(*B, Code, Tsm);
}

// ── EmitBlock（對應 Godot SpellCompiler.EmitBlock）──────────────

void FSpellCompiler::EmitBlock(const FBlockNode& Block,
                                TArray<FInstruction>& Code,
                                const TMap<FName, FName>& Tsm)
{
    switch (Block.Type)
    {
        // ── 直接對應：讀 "args" → 同型別 Payload ─────────────────
        case EBlockType::Wait:
        {
            FWaitArgs A; ReadArgs(Block, A);
            Code.Add(MakeI(EOpCode::Wait, A));
            break;
        }
        case EBlockType::SetVar:
        case EBlockType::GetVar:
        case EBlockType::SetVarBool:
        case EBlockType::GetVarBool:
        {
            FSetVarArgs A; ReadArgs(Block, A);
            Code.Add(MakeI(EOpCode::SetVar, A));
            break;
        }
        case EBlockType::InvokeSpell:
        {
            FInvokeSpellArgs A; ReadArgs(Block, A);
            Code.Add(MakeI(EOpCode::InvokeSpell, A));
            break;
        }
        case EBlockType::InvokeTotem:
        {
            FInvokeTotemArgs A; ReadArgs(Block, A);
            Code.Add(MakeI(EOpCode::InvokeTotem, A));
            break;
        }
        case EBlockType::Sleep:
        {
            FSleepArgs A; ReadArgs(Block, A);
            Code.Add(MakeI(EOpCode::SleepFrames, A));
            break;
        }
        case EBlockType::Compare:
        {
            FStoreCompareArgs A; ReadArgs(Block, A);
            Code.Add(MakeI(EOpCode::StoreCompare, A));
            break;
        }
        case EBlockType::QueryNearest:
        {
            FQueryArgs A; ReadArgs(Block, A);
            Code.Add(MakeI(EOpCode::QueryNearest, A));
            break;
        }
        case EBlockType::QueryNear:
        {
            FQueryArgs A; ReadArgs(Block, A);
            Code.Add(MakeI(EOpCode::QueryNearCount, A));
            break;
        }
        case EBlockType::GetEntityProp:
        {
            FEntityPropArgs A; ReadArgs(Block, A);
            Code.Add(MakeI(EOpCode::GetEntityProp, A));
            break;
        }
        case EBlockType::SetEntityProp:
        {
            FEntityPropArgs A; ReadArgs(Block, A);
            Code.Add(MakeI(EOpCode::StoreEntityProp, A));
            break;
        }
        case EBlockType::Broadcast:
        case EBlockType::BroadcastAndWait:
        {
            FBroadcastArgs A; ReadArgs(Block, A);
            Code.Add(MakeI(EOpCode::Broadcast, A));
            break;
        }
        case EBlockType::OnReceive:
        {
            FBroadcastArgs A; ReadArgs(Block, A);
            Code.Add(MakeI(EOpCode::OnReceive, A));
            break;
        }
        case EBlockType::ListCreate:  { FListArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::ListCreate,  A)); break; }
        case EBlockType::ListAppend:  { FListArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::ListAppend,  A)); break; }
        case EBlockType::ListPop:     { FListArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::ListPop,     A)); break; }
        case EBlockType::ListGet:     { FListArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::ListGet,     A)); break; }
        case EBlockType::ListDequeue: { FListArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::ListDequeue, A)); break; }
        case EBlockType::ListSet:     { FListArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::ListSet,     A)); break; }
        case EBlockType::ListLength:  { FListArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::ListLength,  A)); break; }
        case EBlockType::ListContains:{ FListArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::ListContains,A)); break; }
        case EBlockType::ListRemoveAt:{ FListArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::ListRemoveAt,A)); break; }
        case EBlockType::ListClear:   { FListArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::ListClear,   A)); break; }
        case EBlockType::VecMake:     { FVecMakeArgs   A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::VecMake,    A)); break; }
        case EBlockType::VecGetComp:  { FVecUnopArgs   A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::VecGetComp, A)); break; }
        case EBlockType::VecAdd:      { FVecBinopArgs  A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::VecAdd,     A)); break; }
        case EBlockType::VecSub:      { FVecBinopArgs  A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::VecSub,     A)); break; }
        case EBlockType::VecScale:    { FVecScaleArgs  A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::VecScale,   A)); break; }
        case EBlockType::VecNegate:   { FVecUnopArgs   A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::VecNegate,  A)); break; }
        case EBlockType::VecNorm:     { FVecUnopArgs   A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::VecNorm,    A)); break; }
        case EBlockType::VecLength:   { FVecUnopArgs   A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::VecLength,  A)); break; }
        case EBlockType::VecDot:      { FVecBinopArgs  A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::VecDot,     A)); break; }
        case EBlockType::VecCross:    { FVecBinopArgs  A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::VecCross,   A)); break; }
        case EBlockType::VecFromEntity:{ FVecFromEntityArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::VecFromEntity, A)); break; }
        case EBlockType::Raycast:     { FRaycastArgs   A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::Raycast,    A)); break; }
        case EBlockType::FocalPoint:  { FGetFocalPointArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::GetFocalPoint, A)); break; }
        case EBlockType::GetBattleStat:{ FReadStatArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::ReadBattleStat, A)); break; }
        case EBlockType::RisingEdge:   { FEdgeArgs A; A.Cond = ReadCond(Block); Code.Add(MakeI(EOpCode::EdgeRising,  A)); break; }
        case EBlockType::FallingEdge:  { FEdgeArgs A; A.Cond = ReadCond(Block); Code.Add(MakeI(EOpCode::EdgeFalling, A)); break; }
        case EBlockType::TaskCounterSet:  { FTaskCounterArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::TaskCounterSet,  A)); break; }
        case EBlockType::TaskCounterAdd:  { FTaskCounterArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::TaskCounterAdd,  A)); break; }
        case EBlockType::TaskCounterGet:  { FTaskCounterArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::TaskCounterGet,  A)); break; }
        case EBlockType::TaskCounterReset:{ FTaskCounterArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::TaskCounterReset,A)); break; }
        case EBlockType::DamageShield:    { FRegisterFilterArgs A; ReadArgs(Block, A); A.FilterType = "DamageShield"; Code.Add(MakeI(EOpCode::RegisterFilter, A)); break; }
        case EBlockType::DeathGuard:      { FRegisterFilterArgs A; ReadArgs(Block, A); A.FilterType = "DeathGuard";   Code.Add(MakeI(EOpCode::RegisterFilter, A)); break; }
        case EBlockType::Anchor:   { FSnapshotArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::AnchorSnapshot,   A)); break; }
        case EBlockType::Rollback: { FSnapshotArgs A; ReadArgs(Block, A); Code.Add(MakeI(EOpCode::RollbackSnapshot, A)); break; }
        case EBlockType::Die:
            Code.Add(MakeI(EOpCode::Die, FJumpArgs{}));
            break;

        // ── 執行統計 ──────────────────────────────────────────────
        case EBlockType::LoopcastIndex:
        {
            FReadStatArgs A; ReadArgs(Block, A); A.StatName = "loopcastIndex";
            Code.Add(MakeI(EOpCode::ReadExecStat, A));
            break;
        }
        case EBlockType::SuccessCount:
        {
            FReadStatArgs A; ReadArgs(Block, A); A.StatName = "successCount";
            Code.Add(MakeI(EOpCode::ReadExecStat, A));
            break;
        }
        case EBlockType::GetComboCount:
        {
            FReadStatArgs A; ReadArgs(Block, A); A.StatName = "comboCount";
            Code.Add(MakeI(EOpCode::ReadExecStat, A));
            break;
        }

        // ── 被動觸發條件 ──────────────────────────────────────────
        case EBlockType::DetectHpThreshold:
        {
            FWaitConditionArgs A; ReadArgs(Block, A); A.CondKey = "hpPct";
            Code.Add(MakeI(EOpCode::WaitCondition, A));
            break;
        }
        case EBlockType::DetectMpThreshold:
        {
            FWaitConditionArgs A; ReadArgs(Block, A); A.CondKey = "mpPct";
            Code.Add(MakeI(EOpCode::WaitCondition, A));
            break;
        }
        case EBlockType::DetectHitReceived:
        {
            FWaitConditionArgs A; A.CondKey = "damaged"; A.Threshold = 0.f;
            Code.Add(MakeI(EOpCode::WaitCondition, A));
            break;
        }
        case EBlockType::DetectEntityEnter:
        {
            FWaitConditionArgs A; ReadArgs(Block, A); A.CondKey = "entityInRange";
            Code.Add(MakeI(EOpCode::WaitCondition, A));
            break;
        }

        // ── Activation mode ───────────────────────────────────────
        case EBlockType::SetActivationInstant:
        {
            FSetActivationModeArgs A; A.Mode = 0;
            Code.Add(MakeI(EOpCode::SetActivationMode, A));
            break;
        }
        case EBlockType::SetActivationDeclare:
        {
            FSetActivationModeArgs A; A.Mode = 1;
            Code.Add(MakeI(EOpCode::SetActivationMode, A));
            break;
        }
        case EBlockType::SetActivationSustained:
        {
            FSetActivationModeArgs A; A.Mode = 2;
            Code.Add(MakeI(EOpCode::SetActivationMode, A));
            break;
        }

        // ── 圖騰積木（查 Tsm 轉 SlotRef）─────────────────────────
        case EBlockType::Totem:
        {
            FInvokeTotemArgs A;
            if (!ReadArgs(Block, A))
            {
                // 從 Block.Params["totemId"] 取 FName，查 Tsm 取 SlotRef
                const FInstancedStruct* IdIS = Block.Params.Find(TEXT("totemId"));
                if (!IdIS) break;
                if (const FName* TotemId = IdIS->GetPtr<FName>())
                    if (const FName* SR = Tsm.Find(*TotemId))
                        A.SlotRef = *SR;
            }
            if (!A.SlotRef.IsNone())
                Code.Add(MakeI(EOpCode::InvokeTotem, A));
            break;
        }

        // ── 純 UI 積木（不產生指令）──────────────────────────────
        case EBlockType::Discard:
        case EBlockType::EffectLabel:
        case EBlockType::Engraving:
            break;

        // ── If / Evaluate（條件分支）──────────────────────────────
        case EBlockType::If:
        {
            // JumpIfFalse → ThenBranch → Jump → ElseBranch
            FJumpIfArgs JifA; JifA.Cond = ReadCond(Block); JifA.TargetPC = 0;
            int32 JifIdx = Code.Num();
            Code.Add(MakeI(EOpCode::JumpIfFalse, JifA));
            EmitList(Block.ThenBranch, Code, Tsm);
            int32 JmpIdx = Code.Num();
            Code.Add(MakeJump(0));
            PatchJumpIf(Code, JifIdx, Code.Num());
            EmitList(Block.ElseBranch, Code, Tsm);
            PatchJump(Code, JmpIdx, Code.Num());
            break;
        }

        case EBlockType::Evaluate:
        {
            FJumpIfArgs JifA; JifA.Cond = ReadCond(Block); JifA.TargetPC = 0;
            int32 JifIdx = Code.Num();
            Code.Add(MakeI(EOpCode::JumpIfFalse, JifA));
            EmitList(Block.ThenBranch, Code, Tsm);
            PatchJumpIf(Code, JifIdx, Code.Num());
            break;
        }

        // ── RepeatN ───────────────────────────────────────────────
        case EBlockType::RepeatN:
        {
            FRepeatPushArgs PA; ReadArgs(Block, PA);
            Code.Add(MakeI(EOpCode::RepeatPush, PA));
            int32 LoopStart = Code.Num();
            EmitList(Block.LoopBody, Code, Tsm);
            FRepeatStepArgs SA; SA.LoopStartPC = LoopStart;
            Code.Add(MakeI(EOpCode::RepeatStep, SA));
            break;
        }

        // ── RepeatWhile ───────────────────────────────────────────
        case EBlockType::RepeatWhile:
        {
            int32 LoopStart = Code.Num();
            FWhileArgs WA; WA.Cond = ReadCond(Block); WA.LoopEndPC = 0;
            int32 WhileIdx = Code.Num();
            Code.Add(MakeI(EOpCode::WhileCheck, WA));
            EmitList(Block.LoopBody, Code, Tsm);
            Code.Add(MakeJump(LoopStart));
            PatchWhile(Code, WhileIdx, Code.Num());
            break;
        }

        // ── ForEachNearby ─────────────────────────────────────────
        case EBlockType::ForEachNearby:
        {
            int32 ForIdx = Code.Num();
            FQueryArgs QA; ReadArgs(Block, QA); QA.LoopEndPC = 0;
            Code.Add(MakeI(EOpCode::ForEachStart, QA));
            EmitList(Block.LoopBody, Code, Tsm);
            FForEachStepArgs SA; SA.LoopStartPC = ForIdx;
            Code.Add(MakeI(EOpCode::ForEachStep, SA));
            PatchForEach(Code, ForIdx, Code.Num());
            break;
        }

        // ── SinglePulse（邊緣單次觸發）───────────────────────────
        case EBlockType::SinglePulse:
        {
            FEdgeArgs EA; EA.Cond = ReadCond(Block); EA.TargetPC = 0;
            int32 EdgeIdx = Code.Num();
            Code.Add(MakeI(EOpCode::EdgeSinglePulse, EA));
            EmitList(Block.ThenBranch, Code, Tsm);
            PatchEdge(Code, EdgeIdx, Code.Num());
            break;
        }

        // ── TaskCounterOnReach ────────────────────────────────────
        case EBlockType::TaskCounterOnReach:
        {
            FTaskCounterArgs CA; ReadArgs(Block, CA); CA.TargetPC = 0;
            int32 CounterIdx = Code.Num();
            Code.Add(MakeI(EOpCode::TaskCounterOnReach, CA));
            EmitList(Block.ThenBranch, Code, Tsm);
            PatchCounter(Code, CounterIdx, Code.Num());
            break;
        }

        // ── RandomChoice ─────────────────────────────────────────
        case EBlockType::RandomChoice:
        {
            FRandomJumpArgs RJ;
            int32 RjIdx = Code.Num();
            Code.Add(MakeI(EOpCode::RandomJump, RJ));
            int32 AddrA = Code.Num();
            EmitList(Block.ThenBranch, Code, Tsm);
            int32 JmpIdx = Code.Num();
            Code.Add(MakeJump(0));
            int32 AddrB = Code.Num();
            EmitList(Block.ElseBranch, Code, Tsm);
            // patch RandomJump targets
            if (FRandomJumpArgs* P = Code[RjIdx].Payload.GetMutablePtr<FRandomJumpArgs>())
            {
                P->Targets = { AddrA, AddrB };
            }
            PatchJump(Code, JmpIdx, Code.Num());
            break;
        }

        // ── AlternateTrigger ──────────────────────────────────────
        case EBlockType::AlternateTrigger:
        {
            FAlternateJumpArgs AJ; AJ.EvenPC = 0; AJ.OddPC = 0;
            int32 AjIdx = Code.Num();
            Code.Add(MakeI(EOpCode::AlternateJump, AJ));
            int32 EvenStart = Code.Num();
            EmitList(Block.ThenBranch, Code, Tsm);
            int32 JmpIdx = Code.Num();
            Code.Add(MakeJump(0));
            int32 OddStart = Code.Num();
            EmitList(Block.ElseBranch, Code, Tsm);
            if (FAlternateJumpArgs* P = Code[AjIdx].Payload.GetMutablePtr<FAlternateJumpArgs>())
            {
                P->EvenPC = EvenStart;
                P->OddPC  = OddStart;
            }
            PatchJump(Code, JmpIdx, Code.Num());
            break;
        }

        default:
            UE_LOG(LogTemp, Warning, TEXT("[SpellCompiler] Unhandled BlockType: %d"),
                   (int32)Block.Type);
            break;
    }
}
