#include "SpellRunner.h"
#include "ExecutionLoop.h"

FSpellRunner::FSpellRunner()
    : Loop(MakeUnique<FExecutionLoop>())
{}

FSpellRunner::~FSpellRunner() = default;

void FSpellRunner::Submit(TUniquePtr<FExecutionContext> Ctx)
{
    FActiveEntry Entry;
    Entry.Ctx = MoveTemp(Ctx);
    ActiveContexts.Add(MoveTemp(Entry));
}

void FSpellRunner::PruneAll()
{
    ActiveContexts.Empty();
}

void FSpellRunner::Tick(float DeltaTime)
{
    if (ActiveContexts.Num() == 0) return;
    Loop->ResetTick();

    for (int32 i = ActiveContexts.Num() - 1; i >= 0; --i)
    {
        Advance(ActiveContexts[i], DeltaTime);
        if (ActiveContexts[i].Ctx->IsFinished())
            ActiveContexts.RemoveAt(i);
    }
}

void FSpellRunner::Advance(FActiveEntry& Entry, float DeltaTime)
{
    FExecutionContext& Ctx = *Entry.Ctx;
    float StepDelta = DeltaTime;

    for (int32 Safety = 0; !Ctx.IsFinished() && Safety < FExecutionLoop::MaxStepsPerCast; ++Safety)
    {
        Loop->Step(Ctx, StepDelta);
        StepDelta = 0.f;

        // 等待狀態：本幀停止推進，下幀繼續
        if (Ctx.State == EExecutionState::Waiting           ||
            Ctx.State == EExecutionState::WaitingFrames      ||
            Ctx.State == EExecutionState::WaitingSignal      ||
            Ctx.State == EExecutionState::WaitingCondition   ||
            Ctx.State == EExecutionState::WaitingRisingEdge  ||
            Ctx.State == EExecutionState::WaitingFallingEdge) break;

        if (!Ctx.PendingInvokeTotem.IsNone())
        {
            if (Ctx.InvokeTotemFn) Ctx.InvokeTotemFn(Ctx, Ctx.PendingInvokeTotem);
            else if (OnInvokeTotem) OnInvokeTotem(Ctx, Ctx.PendingInvokeTotem);
            Ctx.PendingInvokeTotem = NAME_None;
            continue;
        }

        if (!Ctx.PendingInvokeSpell.IsNone())
        {
            FName NextSpell = Ctx.PendingInvokeSpell;
            Ctx.PendingInvokeSpell = NAME_None;
            if (OnBuildComboContext && Entry.ComboDepth < 8)
            {
                auto ComboCtx = OnBuildComboContext(NextSpell);
                if (ComboCtx)
                {
                    FActiveEntry ComboEntry;
                    ComboEntry.Ctx        = MoveTemp(ComboCtx);
                    ComboEntry.ComboDepth = Entry.ComboDepth + 1;
                    ActiveContexts.Add(MoveTemp(ComboEntry));
                }
            }
            continue;
        }

        if (Ctx.PendingEntityDamageId >= 0)
        {
            if (OnEntityDamage)
                OnEntityDamage(Ctx.PendingEntityDamageId, Ctx.PendingEntityDamageAmount);
            Ctx.PendingEntityDamageId     = -1;
            Ctx.PendingEntityDamageAmount = 0.f;
            continue;
        }

        if (Ctx.PendingEntityMoveId >= 0)
        {
            if (OnEntityMove)
                OnEntityMove(Ctx.PendingEntityMoveId, Ctx.PendingEntityMovePos);
            Ctx.PendingEntityMoveId = -1;
            continue;
        }
    }
}
