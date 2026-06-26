#include "USpecialStatusComponent.h"
#include "UElementalAuraComponent.h"

USpecialStatusComponent::USpecialStatusComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USpecialStatusComponent::BeginPlay()
{
    Super::BeginPlay();
    ElementalAura = GetOwner()
        ? GetOwner()->FindComponentByClass<UElementalAuraComponent>()
        : nullptr;
}

void USpecialStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    ProcessEffects(DeltaTime);
    RecalcAggregates();
}

// ── 異常狀態 API ──────────────────────────────────────────────────────────────

void USpecialStatusComponent::ApplyStatus(TUniquePtr<FAbnormalStatusEffect> Effect,
                                          IElementalTarget* Target)
{
    if (!Effect) return;
    const FName Id       = Effect->GetStatusId();
    const bool bStack    = Effect->IsStackable();
    const int32 MaxStack = Effect->GetMaxStacks();

    if (!bStack)
    {
        // 不可疊加：找同 Id 的現有層，刷新持續時間
        for (auto& E : ActiveEffects)
        {
            if (E->GetStatusId() == Id)
            {
                E->RemainingDuration = Effect->RemainingDuration;
                RecalcAggregates();
                return;
            }
        }
    }
    else
    {
        // 可疊加：計算現有層數，超過上限則拒絕
        int32 Count = 0;
        for (const auto& E : ActiveEffects)
            if (E->GetStatusId() == Id) ++Count;
        if (Count >= MaxStack) return;
    }

    Effect->OnApply(Target);
    ActiveEffects.Add(MoveTemp(Effect));
    RecalcAggregates();
}

void USpecialStatusComponent::RemoveStatus(FName StatusId, IElementalTarget* Target)
{
    for (int32 i = ActiveEffects.Num() - 1; i >= 0; --i)
    {
        if (ActiveEffects[i]->GetStatusId() == StatusId)
        {
            ActiveEffects[i]->OnRemove(Target);
            ActiveEffects.RemoveAt(i);
        }
    }
    RecalcAggregates();
}

bool USpecialStatusComponent::HasStatus(FName StatusId) const
{
    for (const auto& E : ActiveEffects)
        if (E->GetStatusId() == StatusId) return true;
    return false;
}

int32 USpecialStatusComponent::GetStackCount(FName StatusId) const
{
    int32 Count = 0;
    for (const auto& E : ActiveEffects)
        if (E->GetStatusId() == StatusId) ++Count;
    return Count;
}

// ── 清除 API ──────────────────────────────────────────────────────────────────

void USpecialStatusComponent::ClearAll(IElementalTarget* Target)
{
    for (auto& E : ActiveEffects) E->OnRemove(Target);
    ActiveEffects.Reset();
    RecalcAggregates();
}

void USpecialStatusComponent::ClearCategory(ESpecialStatusCategory Category, IElementalTarget* Target)
{
    for (int32 i = ActiveEffects.Num() - 1; i >= 0; --i)
    {
        if (ActiveEffects[i]->GetCategory() == Category)
        {
            ActiveEffects[i]->OnRemove(Target);
            ActiveEffects.RemoveAt(i);
        }
    }
    RecalcAggregates();
}

void USpecialStatusComponent::ClearOutOfCombat(IElementalTarget* Target)
{
    // TODO: 待 FAbnormalStatusEffect 加上 bClearOnExitCombat 旗標後實作
    // 目前無行為，接線點預留給 UCombatStateSubsystem::ExitCombat
}

// ── 私有 ──────────────────────────────────────────────────────────────────────

void USpecialStatusComponent::ProcessEffects(float DeltaTime)
{
    for (int32 i = ActiveEffects.Num() - 1; i >= 0; --i)
    {
        ActiveEffects[i]->OnProcess(DeltaTime);
        ActiveEffects[i]->RemainingDuration -= DeltaTime;
        if (ActiveEffects[i]->RemainingDuration <= 0.f)
        {
            ActiveEffects[i]->OnRemove(nullptr);
            ActiveEffects.RemoveAt(i);
        }
    }
}

void USpecialStatusComponent::RecalcAggregates()
{
    float Spd = 0.f, DmgBonus = 0.f, DefPen = 0.f;
    float AtkBonus = 0.f, DefBonus = 0.f;
    bool bImmob = false;

    // 元素類（由 UElementalAuraComponent 自帶 tick 維護）
    if (ElementalAura)
    {
        Spd     += ElementalAura->SpeedPenalty;
        DmgBonus+= ElementalAura->DamageTakenBonus;
        DefPen  += ElementalAura->DefensePenalty;
        bImmob  |= ElementalAura->bIsImmobilized;
    }

    // 異常 + 其他
    for (const auto& E : ActiveEffects)
    {
        Spd     += E->GetSpeedPenalty();
        DmgBonus+= E->GetDamageTakenBonus();
        DefPen  += E->GetDefensePenalty();
        AtkBonus+= E->GetAttackBonus();
        DefBonus+= E->GetDefenseBonus();
        bImmob  |= E->GetImmobilizes();
    }

    TotalSpeedPenalty     = Spd;
    TotalDamageTakenBonus = DmgBonus;
    TotalDefensePenalty   = FMath::Min(1.f, DefPen);
    TotalAttackBonus      = AtkBonus;
    TotalDefenseBonus     = DefBonus;
    bIsImmobilized        = bImmob;
}
