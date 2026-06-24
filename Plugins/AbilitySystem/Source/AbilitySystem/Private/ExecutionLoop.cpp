#include "ExecutionLoop.h"

// ── 工具方法 ─────────────────────────────────────────────────────

FName FExecutionLoop::VecKey(FName VecName, TCHAR Comp)
{
    return FName(*FString::Printf(TEXT("%s.%c"), *VecName.ToString(), Comp));
}

void FExecutionLoop::GetVec(const TMap<FName, float>& Vars, FName Name,
                             float& X, float& Y, float& Z)
{
    auto Get = [&](FName K) -> float
    {
        const float* V = Vars.Find(K);
        return V ? *V : 0.f;
    };
    X = Get(VecKey(Name, 'x'));
    Y = Get(VecKey(Name, 'y'));
    Z = Get(VecKey(Name, 'z'));
}

void FExecutionLoop::SetVec(TMap<FName, float>& Vars, FName Name,
                             float X, float Y, float Z)
{
    Vars.FindOrAdd(VecKey(Name, 'x')) = X;
    Vars.FindOrAdd(VecKey(Name, 'y')) = Y;
    Vars.FindOrAdd(VecKey(Name, 'z')) = Z;
}

void FExecutionLoop::SetEntityVars(FExecutionContext& Ctx, const FEntityInfo& E)
{
    Ctx.InstanceVars.FindOrAdd("_e.x")     = (float)E.Position.X;
    Ctx.InstanceVars.FindOrAdd("_e.y")     = (float)E.Position.Y;
    Ctx.InstanceVars.FindOrAdd("_e.z")     = (float)E.Position.Z;
    Ctx.InstanceVars.FindOrAdd("_e.hp")    = E.Hp;
    Ctx.InstanceVars.FindOrAdd("_e.maxhp") = E.MaxHp;
    Ctx.InstanceVars.FindOrAdd("_e.idx")   = (float)E.Id;
}

float FExecutionLoop::ResolveNum(const FNumRef& Ref, const FExecutionContext& Ctx)
{
    if (!Ref.Var.IsNone())
    {
        if (const float* V = Ctx.InstanceVars.Find(Ref.Var))  return *V;
        if (const float* V = FExecutionContext::GlobalVars.Find(Ref.Var)) return *V;
        return 0.f;
    }
    return Ref.Val;
}

bool FExecutionLoop::EvalCondition(const FConditionArgs& Cond,
                                    const FExecutionContext& Ctx) const
{
    switch (Cond.Type)
    {
        case EConditionType::TotemHit:    return Ctx.HitTotems.Contains(Cond.TotemName);
        case EConditionType::TotemDone:   return Ctx.DoneTotems.Contains(Cond.TotemName);
        case EConditionType::TotemFizzle: return Ctx.FizzledTotems.Contains(Cond.TotemName);
        case EConditionType::VarBool:
        {
            if (const float* V = Ctx.InstanceVars.Find(Cond.BoolVar)) return *V != 0.f;
            if (const float* V = FExecutionContext::GlobalVars.Find(Cond.BoolVar)) return *V != 0.f;
            return false;
        }
        default:
            return EvalCompare(Cond, Ctx);
    }
}

bool FExecutionLoop::EvalCompare(const FConditionArgs& Cond,
                                  const FExecutionContext& Ctx) const
{
    float L = ResolveNum(Cond.Left,  Ctx);
    float R = ResolveNum(Cond.Right, Ctx);
    FName Op = Cond.Op;
    if (Op == "==")  return  FMath::IsNearlyEqual(L, R, 0.0001f);
    if (Op == "!=")  return !FMath::IsNearlyEqual(L, R, 0.0001f);
    if (Op == "<")   return L < R;
    if (Op == "<=")  return L <= R;
    if (Op == ">")   return L > R;
    if (Op == ">=")  return L >= R;
    return false;
}

// ── FormatTraceParams（對應 Godot ExecutionLoop.FormatTraceParams）────

FString FExecutionLoop::FormatTraceParams(int32 PC, const FInstruction& Instr)
{
    const UEnum* OpEnum = StaticEnum<EOpCode>();
    FString OpName = OpEnum
        ? OpEnum->GetNameStringByValue(static_cast<int64>(Instr.OpCode))
        : FString::Printf(TEXT("Op%d"), static_cast<int32>(Instr.OpCode));

    FString PayloadStr = TEXT("-");
    if (Instr.Payload.IsValid())
    {
        const UScriptStruct* S = Instr.Payload.GetScriptStruct();
        PayloadStr = S ? S->GetName() : TEXT("?");

        // 常見 payload 的關鍵欄位格式化
        if (const FWaitArgs* W = Instr.Payload.GetPtr<FWaitArgs>())
        {
            PayloadStr += W->Duration.Var.IsNone()
                ? FString::Printf(TEXT("(%.2fs)"), W->Duration.Val)
                : FString::Printf(TEXT("($%s)"), *W->Duration.Var.ToString());
        }
        else if (const FJumpArgs* J = Instr.Payload.GetPtr<FJumpArgs>())
            PayloadStr += FString::Printf(TEXT("(%d)"), J->TargetPC);
        else if (const FSetVarArgs* V = Instr.Payload.GetPtr<FSetVarArgs>())
            PayloadStr += FString::Printf(TEXT("(%s)"), *V->VarName.ToString());
    }

    return FString::Printf(TEXT("PC=%d | %-24s | %s"), PC, *OpName, *PayloadStr);
}

// ── Step（對應 Godot ExecutionLoop.Step）────────────────────────

bool FExecutionLoop::Step(FExecutionContext& Ctx, float DeltaTime)
{
    if (Ctx.IsFinished()) return true;

    // Wait 計時
    if (Ctx.State == EExecutionState::Waiting)
    {
        Ctx.WaitRemaining -= DeltaTime;
        if (Ctx.WaitRemaining > 0.f) return true;
        Ctx.State = EExecutionState::Running;
        ++Ctx.PC;
    }

    // SleepFrames 計幀
    if (Ctx.State == EExecutionState::WaitingFrames)
    {
        --Ctx.WaitFramesRemaining;
        if (Ctx.WaitFramesRemaining > 0) return true;
        Ctx.State = EExecutionState::Running;
        ++Ctx.PC;
    }

    // OnReceive 訊號
    if (Ctx.State == EExecutionState::WaitingSignal)
    {
        bool bHas = Ctx.HasSignalFn && Ctx.HasSignalFn(Ctx.WaitingSignalName);
        if (!bHas) return true;
        Ctx.State = EExecutionState::Running;
        Ctx.WaitingSignalName = NAME_None;
        ++Ctx.PC;
    }

    // WaitCondition
    if (Ctx.State == EExecutionState::WaitingCondition)
    {
        bool bMet = false;
        FName Key = Ctx.WaitingConditionKey;
        if (Key == "damaged")
            bMet = Ctx.TookDamageThisTickFn && Ctx.TookDamageThisTickFn();
        else if (Key == "entityInRange")
            bMet = Ctx.EntityQuery &&
                   Ctx.EntityQuery(Ctx.WaitingConditionThreshold).Num() > 0;
        else if (Ctx.PlayerStatsQuery)
            bMet = Ctx.PlayerStatsQuery(Key) < Ctx.WaitingConditionThreshold;
        else
            bMet = true;  // 無代理（同步路徑）→ 直接通過，對應 Godot ExecutionLoop.cs:65

        if (!bMet) return true;
        Ctx.WaitingConditionKey = NAME_None;
        Ctx.State = EExecutionState::Running;
        ++Ctx.PC;
    }

    // 邊緣觸發（每幀頂部重新評估）
    if (Ctx.State == EExecutionState::WaitingRisingEdge ||
        Ctx.State == EExecutionState::WaitingFallingEdge)
    {
        int32 Epc = Ctx.WaitingEdgePC;
        if (Epc >= 0 && Epc < Ctx.Code.Num())
        {
            if (const FEdgeArgs* A = Ctx.Code[Epc].Payload.GetPtr<FEdgeArgs>())
            {
                bool Cur  = EvalCondition(A->Cond, Ctx);
                bool Prev = false;
                if (bool* P = Ctx.EdgeState.Find(Epc)) Prev = *P;
                Ctx.EdgeState.FindOrAdd(Epc) = Cur;
                bool bMet = (Ctx.State == EExecutionState::WaitingRisingEdge)
                    ? (!Prev && Cur)
                    : ( Prev && !Cur);
                if (bMet)
                {
                    Ctx.WaitingEdgePC = -1;
                    Ctx.State = EExecutionState::Running;
                    Ctx.PC = Epc + 1;
                }
            }
        }
        return true;
    }

    // 主執行迴圈
    while (Ctx.State == EExecutionState::Running && Ctx.PC < Ctx.Code.Num())
    {
        if (ExecutionsThisTick >= MaxOpcodeDispatchPerStep) return false;

        if (FExecutionContext::bTraceMode)
            UE_LOG(LogTemp, Log, TEXT("[VM] %s"), *FormatTraceParams(Ctx.PC, Ctx.Code[Ctx.PC]));
        Execute(Ctx.Code[Ctx.PC], Ctx);
        ++ExecutionsThisTick;

        if (Ctx.IsFinished())                             break;
        if (Ctx.State != EExecutionState::Running)        break;
        if (!Ctx.PendingInvokeSpell.IsNone() ||
            !Ctx.PendingInvokeTotem.IsNone() ||
            Ctx.PendingEntityDamageId >= 0   ||
            Ctx.PendingEntityMoveId   >= 0)               break;
    }

    if (Ctx.State == EExecutionState::Running && Ctx.PC >= Ctx.Code.Num())
        Ctx.State = EExecutionState::Completed;

    return true;
}

// ── Execute 指令分派 ──────────────────────────────────────────────

void FExecutionLoop::Execute(const FInstruction& Instr, FExecutionContext& Ctx)
{
    switch (Instr.OpCode)
    {
        // ── Wait / SleepFrames ────────────────────────────────────
        case EOpCode::Wait:
        {
            const FWaitArgs* A = Instr.Payload.GetPtr<FWaitArgs>();
            float Dur = A ? ResolveNum(A->Duration, Ctx) : 0.f;
            if (Dur > 0.f)
            {
                Ctx.WaitRemaining = Dur;
                Ctx.State = EExecutionState::Waiting;
                return;  // PC 不前進
            }
            ++Ctx.PC;
            break;
        }

        case EOpCode::SleepFrames:
        {
            const FSleepArgs* A = Instr.Payload.GetPtr<FSleepArgs>();
            int32 F = A ? FMath::Max(1, (int32)ResolveNum(A->Frames, Ctx)) : 1;
            Ctx.WaitFramesRemaining = F;
            Ctx.State = EExecutionState::WaitingFrames;
            return;  // PC 不前進
        }

        // ── Jump ──────────────────────────────────────────────────
        case EOpCode::Jump:
        {
            const FJumpArgs* A = Instr.Payload.GetPtr<FJumpArgs>();
            Ctx.PC = A ? A->TargetPC : Ctx.PC + 1;
            break;
        }

        case EOpCode::JumpIfFalse:
        {
            const FJumpIfArgs* A = Instr.Payload.GetPtr<FJumpIfArgs>();
            if (!A) { ++Ctx.PC; break; }
            Ctx.PC = EvalCondition(A->Cond, Ctx) ? Ctx.PC + 1 : A->TargetPC;
            break;
        }

        // ── SetVar ────────────────────────────────────────────────
        case EOpCode::SetVar:
        {
            const FSetVarArgs* A = Instr.Payload.GetPtr<FSetVarArgs>();
            if (A && !A->VarName.IsNone())
            {
                float Val = ResolveNum(A->Value, Ctx);
                if (A->bGlobal)
                    FExecutionContext::GlobalVars.FindOrAdd(A->VarName) = Val;
                else
                    Ctx.InstanceVars.FindOrAdd(A->VarName) = Val;
            }
            ++Ctx.PC;
            break;
        }

        // ── InvokeTotem / InvokeSpell ─────────────────────────────
        case EOpCode::InvokeTotem:
        {
            const FInvokeTotemArgs* A = Instr.Payload.GetPtr<FInvokeTotemArgs>();
            if (A) Ctx.PendingInvokeTotem = A->SlotRef;
            ++Ctx.PC;
            break;
        }

        case EOpCode::InvokeSpell:
        {
            const FInvokeSpellArgs* A = Instr.Payload.GetPtr<FInvokeSpellArgs>();
            if (A) Ctx.PendingInvokeSpell = A->SpellName;
            ++Ctx.PC;
            break;
        }

        // ── RepeatN ───────────────────────────────────────────────
        case EOpCode::RepeatPush:
        {
            const FRepeatPushArgs* A = Instr.Payload.GetPtr<FRepeatPushArgs>();
            int32 Count = A ? FMath::Clamp((int32)ResolveNum(A->Count, Ctx), 1, 20) : 1;
            Ctx.LoopCounters.Push(Count);
            ++Ctx.PC;
            break;
        }

        case EOpCode::RepeatStep:
        {
            if (Ctx.LoopCounters.Num() == 0) { ++Ctx.PC; break; }
            const FRepeatStepArgs* A = Instr.Payload.GetPtr<FRepeatStepArgs>();
            int32 Rem = Ctx.LoopCounters.Pop() - 1;
            if (Rem > 0)
            {
                Ctx.LoopCounters.Push(Rem);
                Ctx.PC = A ? A->LoopStartPC : Ctx.PC + 1;
            }
            else
            {
                ++Ctx.PC;
            }
            break;
        }

        // ── WhileCheck ────────────────────────────────────────────
        case EOpCode::WhileCheck:
        {
            const FWhileArgs* A = Instr.Payload.GetPtr<FWhileArgs>();
            if (!A) { ++Ctx.PC; break; }
            int32  WhilePc = Ctx.PC;
            int32& Iters   = Ctx.WhileIterCounters.FindOrAdd(WhilePc);
            if (++Iters > MaxWhileLoopIterations)
            {
                Ctx.WhileIterCounters.Remove(WhilePc);
                Ctx.State = EExecutionState::Fizzled;
                break;
            }
            if (EvalCondition(A->Cond, Ctx))
                ++Ctx.PC;
            else
            {
                Ctx.WhileIterCounters.Remove(WhilePc);
                Ctx.PC = A->LoopEndPC;
            }
            break;
        }

        // ── StoreCompare ──────────────────────────────────────────
        case EOpCode::StoreCompare:
        {
            const FStoreCompareArgs* A = Instr.Payload.GetPtr<FStoreCompareArgs>();
            if (A && !A->ResultVar.IsNone())
            {
                FConditionArgs Cond;
                Cond.Type  = EConditionType::Compare;
                Cond.Left  = A->Left;
                Cond.Right = A->Right;
                Cond.Op    = A->Op;
                float Val  = EvalCondition(Cond, Ctx) ? 1.f : 0.f;
                if (A->bGlobal)
                    FExecutionContext::GlobalVars.FindOrAdd(A->ResultVar) = Val;
                else
                    Ctx.InstanceVars.FindOrAdd(A->ResultVar) = Val;
            }
            ++Ctx.PC;
            break;
        }

        // ── ForEachNearby ─────────────────────────────────────────
        case EOpCode::ForEachStart:
        {
            const FQueryArgs* A = Instr.Payload.GetPtr<FQueryArgs>();
            if (!A || !Ctx.EntityQuery)
            {
                Ctx.PC = A ? A->LoopEndPC : Ctx.PC + 1;
                break;
            }
            TArray<FEntityInfo> Entities = Ctx.EntityQuery(ResolveNum(A->Radius, Ctx));
            if (Entities.Num() == 0)
            {
                Ctx.PC = A->LoopEndPC;
                break;
            }
            FEntityIterFrame Frame;
            Frame.Entities     = MoveTemp(Entities);
            Frame.CurrentIndex = 0;
            Ctx.EntityIterators.Push(MoveTemp(Frame));
            Ctx.CurrentIterEntity = Ctx.EntityIterators.Last().Entities[0];
            SetEntityVars(Ctx, *Ctx.CurrentIterEntity);
            ++Ctx.PC;
            break;
        }

        case EOpCode::ForEachStep:
        {
            const FForEachStepArgs* A = Instr.Payload.GetPtr<FForEachStepArgs>();
            if (Ctx.EntityIterators.Num() == 0) { ++Ctx.PC; break; }
            FEntityIterFrame& Frame = Ctx.EntityIterators.Last();
            int32 Next = Frame.CurrentIndex + 1;
            if (Next < Frame.Entities.Num())
            {
                Frame.CurrentIndex = Next;
                Ctx.CurrentIterEntity = Frame.Entities[Next];
                SetEntityVars(Ctx, *Ctx.CurrentIterEntity);
                Ctx.PC = A ? A->LoopStartPC : Ctx.PC + 1;
            }
            else
            {
                Ctx.EntityIterators.Pop();
                Ctx.CurrentIterEntity.Reset();
                ++Ctx.PC;
            }
            break;
        }

        // ── QueryNearest ─────────────────────────────────────────
        case EOpCode::QueryNearest:
        {
            const FQueryArgs* A = Instr.Payload.GetPtr<FQueryArgs>();
            if (A && Ctx.EntityQuery)
            {
                FName RV = A->ResultVar.IsNone() ? FName("nearest") : A->ResultVar;
                TArray<FEntityInfo> List = Ctx.EntityQuery(ResolveNum(A->Radius, Ctx));
                FString RVStr = RV.ToString();
                if (List.Num() > 0)
                {
                    const FEntityInfo& E = List[0];
                    Ctx.InstanceVars.FindOrAdd(FName(*(RVStr + ".found")))  = 1.f;
                    Ctx.InstanceVars.FindOrAdd(FName(*(RVStr + ".x")))      = (float)E.Position.X;
                    Ctx.InstanceVars.FindOrAdd(FName(*(RVStr + ".y")))      = (float)E.Position.Y;
                    Ctx.InstanceVars.FindOrAdd(FName(*(RVStr + ".z")))      = (float)E.Position.Z;
                    Ctx.InstanceVars.FindOrAdd(FName(*(RVStr + ".hp")))     = E.Hp;
                    Ctx.InstanceVars.FindOrAdd(FName(*(RVStr + ".maxhp")))  = E.MaxHp;
                }
                else
                {
                    Ctx.InstanceVars.FindOrAdd(FName(*(RVStr + ".found"))) = 0.f;
                }
            }
            ++Ctx.PC;
            break;
        }

        case EOpCode::QueryNearCount:
        {
            const FQueryArgs* A = Instr.Payload.GetPtr<FQueryArgs>();
            if (A && !A->ResultVar.IsNone())
            {
                float Cnt = Ctx.EntityQuery
                    ? (float)Ctx.EntityQuery(ResolveNum(A->Radius, Ctx)).Num()
                    : 0.f;
                Ctx.InstanceVars.FindOrAdd(A->ResultVar) = Cnt;
            }
            ++Ctx.PC;
            break;
        }

        // ── GetEntityProp / StoreEntityProp ───────────────────────
        case EOpCode::GetEntityProp:
        {
            const FEntityPropArgs* A = Instr.Payload.GetPtr<FEntityPropArgs>();
            if (A && !A->ResultVar.IsNone() && Ctx.CurrentIterEntity.IsSet())
            {
                const FEntityInfo& E = *Ctx.CurrentIterEntity;
                float Val = 0.f;
                if      (A->PropName == "hp")    Val = E.Hp;
                else if (A->PropName == "maxhp") Val = E.MaxHp;
                else if (A->PropName == "x")     Val = (float)E.Position.X;
                else if (A->PropName == "y")     Val = (float)E.Position.Y;
                else if (A->PropName == "z")     Val = (float)E.Position.Z;
                Ctx.InstanceVars.FindOrAdd(A->ResultVar) = Val;
            }
            ++Ctx.PC;
            break;
        }

        case EOpCode::StoreEntityProp:
        {
            const FEntityPropArgs* A = Instr.Payload.GetPtr<FEntityPropArgs>();
            if (!A || !Ctx.CurrentIterEntity.IsSet()) { ++Ctx.PC; break; }
            const FEntityInfo& E = *Ctx.CurrentIterEntity;
            float Val = ResolveNum(A->Value, Ctx);
            if (A->PropName == "hp" && Val > 0.f)
            {
                Ctx.PendingEntityDamageId     = E.Id;
                Ctx.PendingEntityDamageAmount = Val;
            }
            else if (A->PropName == "x")
            {
                Ctx.PendingEntityMoveId  = E.Id;
                Ctx.PendingEntityMovePos = FGridPos((int32)Val, E.Position.Y, E.Position.Z);
            }
            else if (A->PropName == "y")
            {
                Ctx.PendingEntityMoveId  = E.Id;
                Ctx.PendingEntityMovePos = FGridPos(E.Position.X, (int32)Val, E.Position.Z);
            }
            else if (A->PropName == "z")
            {
                Ctx.PendingEntityMoveId  = E.Id;
                Ctx.PendingEntityMovePos = FGridPos(E.Position.X, E.Position.Y, (int32)Val);
            }
            ++Ctx.PC;
            break;
        }

        // ── Broadcast / OnReceive ─────────────────────────────────
        case EOpCode::Broadcast:
        {
            const FBroadcastArgs* A = Instr.Payload.GetPtr<FBroadcastArgs>();
            if (A && Ctx.BroadcastFn) Ctx.BroadcastFn(A->SignalName);
            ++Ctx.PC;
            break;
        }

        case EOpCode::OnReceive:
        {
            const FBroadcastArgs* A = Instr.Payload.GetPtr<FBroadcastArgs>();
            bool bAlreadyFired = !A || A->SignalName.IsNone() ||
                                 (Ctx.HasSignalFn && Ctx.HasSignalFn(A->SignalName));
            if (bAlreadyFired)
                ++Ctx.PC;
            else
            {
                Ctx.WaitingSignalName = A->SignalName;
                Ctx.State = EExecutionState::WaitingSignal;
            }
            break;
        }

        // ── Die ───────────────────────────────────────────────────
        case EOpCode::Die:
            Ctx.State = EExecutionState::Completed;
            break;

        // ── List 操作 ─────────────────────────────────────────────
        case EOpCode::ListCreate:
        {
            const FListArgs* A = Instr.Payload.GetPtr<FListArgs>();
            if (A && !A->ListName.IsNone()) Ctx.GetOrCreateList(A->ListName, A->bGlobal);
            ++Ctx.PC;
            break;
        }

        case EOpCode::ListAppend:
        {
            const FListArgs* A = Instr.Payload.GetPtr<FListArgs>();
            if (A && !A->ListName.IsNone())
                Ctx.GetOrCreateList(A->ListName, A->bGlobal).Add(ResolveNum(A->Value, Ctx));
            ++Ctx.PC;
            break;
        }

        case EOpCode::ListPop:
        {
            const FListArgs* A = Instr.Payload.GetPtr<FListArgs>();
            if (A && !A->ListName.IsNone())
            {
                if (TArray<float>* L = Ctx.GetList(A->ListName, A->bGlobal))
                {
                    if (L->Num() > 0)
                    {
                        float V = L->Last(); L->Pop();
                        if (!A->ResultVar.IsNone())
                            Ctx.InstanceVars.FindOrAdd(A->ResultVar) = V;
                    }
                }
            }
            ++Ctx.PC;
            break;
        }

        case EOpCode::ListDequeue:
        {
            const FListArgs* A = Instr.Payload.GetPtr<FListArgs>();
            if (A && !A->ListName.IsNone())
            {
                if (TArray<float>* L = Ctx.GetList(A->ListName, A->bGlobal))
                {
                    if (L->Num() > 0)
                    {
                        float V = (*L)[0]; L->RemoveAt(0);
                        if (!A->ResultVar.IsNone())
                            Ctx.InstanceVars.FindOrAdd(A->ResultVar) = V;
                    }
                }
            }
            ++Ctx.PC;
            break;
        }

        case EOpCode::ListGet:
        {
            const FListArgs* A = Instr.Payload.GetPtr<FListArgs>();
            if (A && !A->ListName.IsNone() && !A->ResultVar.IsNone())
            {
                if (TArray<float>* L = Ctx.GetList(A->ListName, A->bGlobal))
                {
                    int32 Idx = (int32)ResolveNum(A->Index, Ctx) - 1;
                    if (L->IsValidIndex(Idx))
                        Ctx.InstanceVars.FindOrAdd(A->ResultVar) = (*L)[Idx];
                }
            }
            ++Ctx.PC;
            break;
        }

        case EOpCode::ListSet:
        {
            const FListArgs* A = Instr.Payload.GetPtr<FListArgs>();
            if (A && !A->ListName.IsNone())
            {
                if (TArray<float>* L = Ctx.GetList(A->ListName, A->bGlobal))
                {
                    int32 Idx = (int32)ResolveNum(A->Index, Ctx) - 1;
                    if (L->IsValidIndex(Idx)) (*L)[Idx] = ResolveNum(A->Value, Ctx);
                }
            }
            ++Ctx.PC;
            break;
        }

        case EOpCode::ListLength:
        {
            const FListArgs* A = Instr.Payload.GetPtr<FListArgs>();
            if (A && !A->ResultVar.IsNone())
            {
                TArray<float>* L = Ctx.GetList(A->ListName, A->bGlobal);
                Ctx.InstanceVars.FindOrAdd(A->ResultVar) = L ? (float)L->Num() : 0.f;
            }
            ++Ctx.PC;
            break;
        }

        case EOpCode::ListContains:
        {
            const FListArgs* A = Instr.Payload.GetPtr<FListArgs>();
            if (A && !A->ResultVar.IsNone())
            {
                float Search = ResolveNum(A->Value, Ctx);
                TArray<float>* L = Ctx.GetList(A->ListName, A->bGlobal);
                bool bFound = L && L->ContainsByPredicate([Search](float V)
                {
                    return FMath::IsNearlyEqual(V, Search, 0.0001f);
                });
                Ctx.InstanceVars.FindOrAdd(A->ResultVar) = bFound ? 1.f : 0.f;
            }
            ++Ctx.PC;
            break;
        }

        case EOpCode::ListRemoveAt:
        {
            const FListArgs* A = Instr.Payload.GetPtr<FListArgs>();
            if (A && !A->ListName.IsNone())
            {
                if (TArray<float>* L = Ctx.GetList(A->ListName, A->bGlobal))
                {
                    int32 Idx = (int32)ResolveNum(A->Index, Ctx) - 1;
                    if (L->IsValidIndex(Idx)) L->RemoveAt(Idx);
                }
            }
            ++Ctx.PC;
            break;
        }

        case EOpCode::ListClear:
        {
            const FListArgs* A = Instr.Payload.GetPtr<FListArgs>();
            if (A)
                if (TArray<float>* L = Ctx.GetList(A->ListName, A->bGlobal))
                    L->Empty();
            ++Ctx.PC;
            break;
        }

        // ── 任務計數器 ────────────────────────────────────────────
        case EOpCode::TaskCounterSet:
        {
            const FTaskCounterArgs* A = Instr.Payload.GetPtr<FTaskCounterArgs>();
            if (A && !A->CounterName.IsNone())
                FExecutionContext::TaskCounters.FindOrAdd(A->CounterName) =
                    ResolveNum(A->Value, Ctx);
            ++Ctx.PC;
            break;
        }

        case EOpCode::TaskCounterAdd:
        {
            const FTaskCounterArgs* A = Instr.Payload.GetPtr<FTaskCounterArgs>();
            if (A && !A->CounterName.IsNone())
                FExecutionContext::TaskCounters.FindOrAdd(A->CounterName) +=
                    ResolveNum(A->Value, Ctx);
            ++Ctx.PC;
            break;
        }

        case EOpCode::TaskCounterGet:
        {
            const FTaskCounterArgs* A = Instr.Payload.GetPtr<FTaskCounterArgs>();
            if (A && !A->ResultVar.IsNone())
            {
                float Val = 0.f;
                if (const float* V = FExecutionContext::TaskCounters.Find(A->CounterName))
                    Val = *V;
                if (A->bGlobal) FExecutionContext::GlobalVars.FindOrAdd(A->ResultVar) = Val;
                else            Ctx.InstanceVars.FindOrAdd(A->ResultVar) = Val;
            }
            ++Ctx.PC;
            break;
        }

        case EOpCode::TaskCounterOnReach:
        {
            const FTaskCounterArgs* A = Instr.Payload.GetPtr<FTaskCounterArgs>();
            if (!A) { ++Ctx.PC; break; }
            float Threshold = ResolveNum(A->Value, Ctx);
            FName ReachedKey = FName(*(A->CounterName.ToString() +
                TEXT("_") + FString::SanitizeFloat(Threshold)));
            float Cur = 0.f;
            if (const float* V = FExecutionContext::TaskCounters.Find(A->CounterName))
                Cur = *V;
            if (Cur >= Threshold &&
                !FExecutionContext::TaskCounterReached.Contains(ReachedKey))
            {
                FExecutionContext::TaskCounterReached.Add(ReachedKey);
                ++Ctx.PC;
            }
            else
            {
                Ctx.PC = A->TargetPC;
            }
            break;
        }

        case EOpCode::TaskCounterReset:
        {
            const FTaskCounterArgs* A = Instr.Payload.GetPtr<FTaskCounterArgs>();
            if (A && !A->CounterName.IsNone())
            {
                FExecutionContext::TaskCounters.FindOrAdd(A->CounterName) = 0.f;
                FString Prefix = A->CounterName.ToString() + TEXT("_");
                TArray<FName> ToRemove;
                for (const FName& K : FExecutionContext::TaskCounterReached)
                    if (K.ToString().StartsWith(Prefix)) ToRemove.Add(K);
                for (const FName& K : ToRemove)
                    FExecutionContext::TaskCounterReached.Remove(K);
            }
            ++Ctx.PC;
            break;
        }

        // ── 向量運算 ─────────────────────────────────────────────

        case EOpCode::VecMake:
        {
            const FVecMakeArgs* A = Instr.Payload.GetPtr<FVecMakeArgs>();
            if (!A) { ++Ctx.PC; break; }
            auto& Vars = A->bGlobal ? FExecutionContext::GlobalVars : Ctx.InstanceVars;
            SetVec(Vars, A->ResultName,
                   ResolveNum(A->X, Ctx), ResolveNum(A->Y, Ctx), ResolveNum(A->Z, Ctx));
            ++Ctx.PC;
            break;
        }

        case EOpCode::VecGetComp:
        {
            const FVecUnopArgs* A = Instr.Payload.GetPtr<FVecUnopArgs>();
            if (!A || A->Result.IsNone()) { ++Ctx.PC; break; }
            auto& Vars = A->bGlobal ? FExecutionContext::GlobalVars : Ctx.InstanceVars;
            float X, Y, Z;
            GetVec(Vars, A->Vec, X, Y, Z);
            float Val = (A->Component == "y") ? Y : (A->Component == "z") ? Z : X;
            Ctx.InstanceVars.FindOrAdd(A->Result) = Val;
            ++Ctx.PC;
            break;
        }

        case EOpCode::VecAdd:
        {
            const FVecBinopArgs* A = Instr.Payload.GetPtr<FVecBinopArgs>();
            if (!A) { ++Ctx.PC; break; }
            auto& Vars = A->bGlobal ? FExecutionContext::GlobalVars : Ctx.InstanceVars;
            float Ax, Ay, Az, Bx, By, Bz;
            GetVec(Vars, A->VecA, Ax, Ay, Az);
            GetVec(Vars, A->VecB, Bx, By, Bz);
            SetVec(Vars, A->Result, Ax+Bx, Ay+By, Az+Bz);
            ++Ctx.PC;
            break;
        }

        case EOpCode::VecSub:
        {
            const FVecBinopArgs* A = Instr.Payload.GetPtr<FVecBinopArgs>();
            if (!A) { ++Ctx.PC; break; }
            auto& Vars = A->bGlobal ? FExecutionContext::GlobalVars : Ctx.InstanceVars;
            float Ax, Ay, Az, Bx, By, Bz;
            GetVec(Vars, A->VecA, Ax, Ay, Az);
            GetVec(Vars, A->VecB, Bx, By, Bz);
            SetVec(Vars, A->Result, Ax-Bx, Ay-By, Az-Bz);
            ++Ctx.PC;
            break;
        }

        case EOpCode::VecScale:
        {
            const FVecScaleArgs* A = Instr.Payload.GetPtr<FVecScaleArgs>();
            if (!A) { ++Ctx.PC; break; }
            auto& Vars = A->bGlobal ? FExecutionContext::GlobalVars : Ctx.InstanceVars;
            float X, Y, Z;
            GetVec(Vars, A->Vec, X, Y, Z);
            float S = ResolveNum(A->Scalar, Ctx);
            SetVec(Vars, A->Result, X*S, Y*S, Z*S);
            ++Ctx.PC;
            break;
        }

        case EOpCode::VecNegate:
        {
            const FVecUnopArgs* A = Instr.Payload.GetPtr<FVecUnopArgs>();
            if (!A) { ++Ctx.PC; break; }
            auto& Vars = A->bGlobal ? FExecutionContext::GlobalVars : Ctx.InstanceVars;
            float X, Y, Z;
            GetVec(Vars, A->Vec, X, Y, Z);
            SetVec(Vars, A->Result, -X, -Y, -Z);
            ++Ctx.PC;
            break;
        }

        case EOpCode::VecNorm:
        {
            const FVecUnopArgs* A = Instr.Payload.GetPtr<FVecUnopArgs>();
            if (!A) { ++Ctx.PC; break; }
            auto& Vars = A->bGlobal ? FExecutionContext::GlobalVars : Ctx.InstanceVars;
            float X, Y, Z;
            GetVec(Vars, A->Vec, X, Y, Z);
            float Len = FMath::Sqrt(X*X + Y*Y + Z*Z);
            if (Len > 0.0001f) { X /= Len; Y /= Len; Z /= Len; }
            else               { X = Y = Z = 0.f; }
            SetVec(Vars, A->Result, X, Y, Z);
            ++Ctx.PC;
            break;
        }

        case EOpCode::VecLength:
        {
            const FVecUnopArgs* A = Instr.Payload.GetPtr<FVecUnopArgs>();
            if (!A || A->Result.IsNone()) { ++Ctx.PC; break; }
            auto& Vars = A->bGlobal ? FExecutionContext::GlobalVars : Ctx.InstanceVars;
            float X, Y, Z;
            GetVec(Vars, A->Vec, X, Y, Z);
            Ctx.InstanceVars.FindOrAdd(A->Result) = FMath::Sqrt(X*X + Y*Y + Z*Z);
            ++Ctx.PC;
            break;
        }

        case EOpCode::VecDot:
        {
            const FVecBinopArgs* A = Instr.Payload.GetPtr<FVecBinopArgs>();
            if (!A || A->Result.IsNone()) { ++Ctx.PC; break; }
            auto& Vars = A->bGlobal ? FExecutionContext::GlobalVars : Ctx.InstanceVars;
            float Ax, Ay, Az, Bx, By, Bz;
            GetVec(Vars, A->VecA, Ax, Ay, Az);
            GetVec(Vars, A->VecB, Bx, By, Bz);
            Ctx.InstanceVars.FindOrAdd(A->Result) = Ax*Bx + Ay*By + Az*Bz;
            ++Ctx.PC;
            break;
        }

        case EOpCode::VecCross:
        {
            // Godot ExecutionLoop.cs:697-710: GetVec 只取 (ax,ay) 二維分量，
            // 結果 = ax*by - ay*bx（純量），存入 InstanceVars[resultVar]（非向量三鍵）。
            // 原 UE5 誤用 SetVec 存 3D 向量（Result.x/y/z），讀取端拿 Result 純量時永遠得 0。
            const FVecBinopArgs* A = Instr.Payload.GetPtr<FVecBinopArgs>();
            if (!A || A->Result.IsNone()) { ++Ctx.PC; break; }
            auto& Vars = A->bGlobal ? FExecutionContext::GlobalVars : Ctx.InstanceVars;
            float Ax, Ay, Az, Bx, By, Bz;
            GetVec(Vars, A->VecA, Ax, Ay, Az);
            GetVec(Vars, A->VecB, Bx, By, Bz);
            Vars.FindOrAdd(A->Result) = Ax*By - Ay*Bx;
            ++Ctx.PC;
            break;
        }

        case EOpCode::VecFromEntity:
        {
            const FVecFromEntityArgs* A = Instr.Payload.GetPtr<FVecFromEntityArgs>();
            if (!A) { ++Ctx.PC; break; }
            auto& Vars = A->bGlobal ? FExecutionContext::GlobalVars : Ctx.InstanceVars;
            if (Ctx.CurrentIterEntity.IsSet())
            {
                const FEntityInfo& E = *Ctx.CurrentIterEntity;
                SetVec(Vars, A->ResultVec,
                       (float)E.Position.X, (float)E.Position.Y, (float)E.Position.Z);
            }
            else
            {
                float Ex = 0.f, Ey = 0.f, Ez = 0.f;
                if (const float* V = Ctx.InstanceVars.Find("_e.x")) Ex = *V;
                if (const float* V = Ctx.InstanceVars.Find("_e.y")) Ey = *V;
                if (const float* V = Ctx.InstanceVars.Find("_e.z")) Ez = *V;
                SetVec(Vars, A->ResultVec, Ex, Ey, Ez);
            }
            ++Ctx.PC;
            break;
        }

        case EOpCode::GetFocalPoint:
        {
            const FGetFocalPointArgs* A = Instr.Payload.GetPtr<FGetFocalPointArgs>();
            if (!A) { ++Ctx.PC; break; }
            auto& Vars = A->bGlobal ? FExecutionContext::GlobalVars : Ctx.InstanceVars;
            FGridPos Fp = Ctx.FocalPointQuery ? Ctx.FocalPointQuery() : FGridPos();
            SetVec(Vars, A->ResultVec, (float)Fp.X, (float)Fp.Y, (float)Fp.Z);
            ++Ctx.PC;
            break;
        }

        case EOpCode::Raycast:
        {
            const FRaycastArgs* A = Instr.Payload.GetPtr<FRaycastArgs>();
            if (!A || !Ctx.RaycastQuery) { ++Ctx.PC; break; }
            auto& Vars = A->bGlobal ? FExecutionContext::GlobalVars : Ctx.InstanceVars;
            float Sx, Sy, Sz, Dx, Dy, Dz;
            GetVec(Vars, A->StartVec, Sx, Sy, Sz);
            GetVec(Vars, A->DirVec,   Dx, Dy, Dz);
            FRaycastResult R = Ctx.RaycastQuery(
                FGridPos((int32)Sx, (int32)Sy, (int32)Sz),
                FVector(Dx, Dy, Dz),
                ResolveNum(A->MaxDist, Ctx));
            SetVec(Vars, A->ResultVec,
                   (float)R.HitPos.X, (float)R.HitPos.Y, (float)R.HitPos.Z);
            FString RV = A->ResultVec.ToString();
            Vars.FindOrAdd(FName(*(RV + ".hit"))) = R.bHit  ? 1.f : 0.f;
            Vars.FindOrAdd(FName(*(RV + ".mat"))) = (float)R.MatId;
            ++Ctx.PC;
            break;
        }

        // ── ReadBattleStat / ReadExecStat ─────────────────────────
        case EOpCode::ReadBattleStat:
        {
            const FReadStatArgs* A = Instr.Payload.GetPtr<FReadStatArgs>();
            if (A && !A->ResultVar.IsNone())
            {
                float Val = Ctx.BattleStatFn ? Ctx.BattleStatFn(A->StatName) : 0.f;
                if (A->bGlobal) FExecutionContext::GlobalVars.FindOrAdd(A->ResultVar) = Val;
                else            Ctx.InstanceVars.FindOrAdd(A->ResultVar) = Val;
            }
            ++Ctx.PC;
            break;
        }

        case EOpCode::ReadExecStat:
        {
            const FReadStatArgs* A = Instr.Payload.GetPtr<FReadStatArgs>();
            if (A && !A->ResultVar.IsNone())
            {
                float Val = 0.f;
                if (A->StatName == "loopcastIndex")
                    Val = (float)Ctx.LoopcastIndex;
                else if (A->StatName == "successCount" || A->StatName == "comboCount")
                    Val = (float)Ctx.HitTotems.Num();
                if (A->bGlobal) FExecutionContext::GlobalVars.FindOrAdd(A->ResultVar) = Val;
                else            Ctx.InstanceVars.FindOrAdd(A->ResultVar) = Val;
            }
            ++Ctx.PC;
            break;
        }

        // ── WaitCondition ─────────────────────────────────────────
        case EOpCode::WaitCondition:
        {
            const FWaitConditionArgs* A = Instr.Payload.GetPtr<FWaitConditionArgs>();
            if (A)
            {
                Ctx.WaitingConditionKey       = A->CondKey;
                Ctx.WaitingConditionThreshold = A->Threshold;
                Ctx.State = EExecutionState::WaitingCondition;
                // PC 不前進：Step 頂部條件成立後 PC++
            }
            break;
        }

        // ── Edge triggers ─────────────────────────────────────────
        case EOpCode::EdgeRising:
        {
            const FEdgeArgs* A = Instr.Payload.GetPtr<FEdgeArgs>();
            if (!A) { ++Ctx.PC; break; }
            Ctx.EdgeState.FindOrAdd(Ctx.PC) = EvalCondition(A->Cond, Ctx);
            Ctx.WaitingEdgePC = Ctx.PC;
            Ctx.State = EExecutionState::WaitingRisingEdge;
            break;
        }

        case EOpCode::EdgeFalling:
        {
            const FEdgeArgs* A = Instr.Payload.GetPtr<FEdgeArgs>();
            if (!A) { ++Ctx.PC; break; }
            Ctx.EdgeState.FindOrAdd(Ctx.PC) = EvalCondition(A->Cond, Ctx);
            Ctx.WaitingEdgePC = Ctx.PC;
            Ctx.State = EExecutionState::WaitingFallingEdge;
            break;
        }

        case EOpCode::EdgeSinglePulse:
        {
            const FEdgeArgs* A = Instr.Payload.GetPtr<FEdgeArgs>();
            if (!A) { ++Ctx.PC; break; }
            int32 Pc = Ctx.PC;
            bool  bCur   = EvalCondition(A->Cond, Ctx);
            bool  bArmed = Ctx.PulseArmed.Contains(Pc);
            if (!bCur)
            {
                Ctx.PulseArmed.Add(Pc);
                Ctx.PC = A->TargetPC;
            }
            else if (bArmed)
            {
                Ctx.PulseArmed.Remove(Pc);
                ++Ctx.PC;
            }
            else
            {
                Ctx.PC = A->TargetPC;
            }
            break;
        }

        // ── RandomJump ────────────────────────────────────────────
        case EOpCode::RandomJump:
        {
            const FRandomJumpArgs* A = Instr.Payload.GetPtr<FRandomJumpArgs>();
            if (A && A->Targets.Num() > 0)
                Ctx.PC = A->Targets[FMath::RandRange(0, A->Targets.Num() - 1)];
            else
                ++Ctx.PC;
            break;
        }

        // ── AlternateJump ─────────────────────────────────────────
        case EOpCode::AlternateJump:
        {
            const FAlternateJumpArgs* A = Instr.Payload.GetPtr<FAlternateJumpArgs>();
            if (!A) { ++Ctx.PC; break; }
            int32& Calls = Ctx.AlternateCounts.FindOrAdd(Ctx.PC);
            Ctx.PC = ((Calls++ % 2) == 0) ? A->EvenPC : A->OddPC;
            break;
        }

        // ── SetActivationMode ─────────────────────────────────────
        case EOpCode::SetActivationMode:
        {
            const FSetActivationModeArgs* A = Instr.Payload.GetPtr<FSetActivationModeArgs>();
            if (A && Ctx.SetActivationModeFn) Ctx.SetActivationModeFn(A->Mode);
            ++Ctx.PC;
            break;
        }

        // ── RegisterFilter ────────────────────────────────────────
        case EOpCode::RegisterFilter:
        {
            const FRegisterFilterArgs* A = Instr.Payload.GetPtr<FRegisterFilterArgs>();
            if (A && Ctx.RegisterFilterFn)
                Ctx.RegisterFilterFn(A->FilterType, A->Mode,
                                     A->bOneShot, A->Threshold, A->CapValue);
            ++Ctx.PC;
            break;
        }

        // ── Snapshot ─────────────────────────────────────────────
        case EOpCode::AnchorSnapshot:
        {
            const FSnapshotArgs* A = Instr.Payload.GetPtr<FSnapshotArgs>();
            if (Ctx.AnchorAction)
                Ctx.AnchorAction(A ? FMath::RoundToInt(ResolveNum(A->Radius, Ctx)) : 10);
            ++Ctx.PC;
            break;
        }

        case EOpCode::RollbackSnapshot:
        {
            if (Ctx.RollbackAction) Ctx.RollbackAction();
            ++Ctx.PC;
            break;
        }

        default:
            UE_LOG(LogTemp, Warning, TEXT("[ExecutionLoop] Unknown OpCode %d at PC=%d — instruction skipped"),
                (int32)Instr.OpCode, Ctx.PC);
            ++Ctx.PC;
            break;
    }
}
