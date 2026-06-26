#include "USpecialStatusComponent.h"
#include "UElementalAuraComponent.h"
#include "AbnormalStatusEffects.h"

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
    // CachedTarget 由 Owner 在其 BeginPlay 呼叫 SetOwnerTarget(this) 設定
}

void USpecialStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    FrostbiteToFrozenCooldown -= DeltaTime;
    ProcessEffects(DeltaTime);
    CheckChainTriggers();
    RecalcAggregates();
}

// ── 異常狀態 API ──────────────────────────────────────────────────────────────

void USpecialStatusComponent::ApplyStatus(TUniquePtr<FAbnormalStatusEffect> Effect,
                                          IElementalTarget* Target)
{
    if (!Effect) return;
    IElementalTarget* T  = Target ? Target : CachedTarget;
    const FName Id       = Effect->GetStatusId();
    const bool bStack    = Effect->IsStackable();
    const int32 MaxStack = Effect->GetMaxStacks();

    if (!bStack)
    {
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
        int32 Count = 0;
        for (const auto& E : ActiveEffects)
            if (E->GetStatusId() == Id) ++Count;
        if (Count >= MaxStack) return;
    }

    Effect->OnApply(T);
    ActiveEffects.Add(MoveTemp(Effect));
    RecalcAggregates();
}

void USpecialStatusComponent::RemoveStatus(FName StatusId, IElementalTarget* Target)
{
    IElementalTarget* T = Target ? Target : CachedTarget;
    for (int32 i = ActiveEffects.Num() - 1; i >= 0; --i)
    {
        if (ActiveEffects[i]->GetStatusId() == StatusId)
        {
            ActiveEffects[i]->OnRemove(T);
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
    IElementalTarget* T = Target ? Target : CachedTarget;
    for (auto& E : ActiveEffects) E->OnRemove(T);
    ActiveEffects.Reset();
    RecalcAggregates();
}

void USpecialStatusComponent::ClearCategory(ESpecialStatusCategory Category, IElementalTarget* Target)
{
    IElementalTarget* T = Target ? Target : CachedTarget;
    for (int32 i = ActiveEffects.Num() - 1; i >= 0; --i)
    {
        if (ActiveEffects[i]->GetCategory() == Category)
        {
            ActiveEffects[i]->OnRemove(T);
            ActiveEffects.RemoveAt(i);
        }
    }
    RecalcAggregates();
}

void USpecialStatusComponent::ClearOutOfCombat(IElementalTarget* Target)
{
    // TODO: 待 FAbnormalStatusEffect 加上 bClearOnExitCombat 旗標後實作
}

// ── 私有 ──────────────────────────────────────────────────────────────────────

void USpecialStatusComponent::ProcessEffects(float DeltaTime)
{
    for (int32 i = ActiveEffects.Num() - 1; i >= 0; --i)
    {
        ActiveEffects[i]->OnProcess(DeltaTime, CachedTarget);
        ActiveEffects[i]->RemainingDuration -= DeltaTime;
        if (ActiveEffects[i]->RemainingDuration <= 0.f)
        {
            ActiveEffects[i]->OnRemove(CachedTarget);
            ActiveEffects.RemoveAt(i);
        }
    }
}

void USpecialStatusComponent::CheckChainTriggers()
{
    // 凍傷 → 結凍（疊滿觸發，5s 免疫冷卻）
    if (HasStatus(AbnormalStatusId::Frostbite))
    {
        int32 FbCount = GetStackCount(AbnormalStatusId::Frostbite);
        int32 FbMax   = GetMaxStacksForStatus(AbnormalStatusId::Frostbite);
        if (FbCount >= FbMax && FrostbiteToFrozenCooldown <= 0.f && !HasStatus(AbnormalStatusId::Frozen))
        {
            ApplyStatus(MakeUnique<FFrozenStatus>(2.f), CachedTarget);
            FrostbiteToFrozenCooldown = 5.f;
        }
    }

    // 恐懼 → 極度恐懼（疊滿觸發；非最大層時移除）
    if (HasStatus(AbnormalStatusId::Fear))
    {
        int32 FearCount = GetStackCount(AbnormalStatusId::Fear);
        int32 FearMax   = GetMaxStacksForStatus(AbnormalStatusId::Fear);
        if (FearCount >= FearMax && !HasStatus(AbnormalStatusId::ExtremeFear))
            ApplyStatus(MakeUnique<FExtremeFearStatus>(), CachedTarget);
        else if (FearCount < FearMax && HasStatus(AbnormalStatusId::ExtremeFear))
            RemoveStatus(AbnormalStatusId::ExtremeFear, CachedTarget);
    }
    else if (HasStatus(AbnormalStatusId::ExtremeFear))
    {
        RemoveStatus(AbnormalStatusId::ExtremeFear, CachedTarget);
    }
}

int32 USpecialStatusComponent::GetMaxStacksForStatus(FName StatusId) const
{
    for (const auto& E : ActiveEffects)
        if (E->GetStatusId() == StatusId) return E->GetMaxStacks();
    return 1;
}

void USpecialStatusComponent::RecalcAggregates()
{
    float Spd = 0.f, DmgBonus = 0.f, DefPen = 0.f;
    float AtkBonus = 0.f, DefBonus = 0.f;
    float AtkPen = 0.f, AtkSpdPen = 0.f, LuckPen = 0.f, MinePen = 0.f;
    bool bImmob = false, bNoSkills = false, bNoMP = false;
    bool bSuperArmor = false, bEthereal = false, bInvinc = false;
    bool bBasicRes = false, bAdvRes = false;

    if (ElementalAura)
    {
        Spd     += ElementalAura->SpeedPenalty;
        DmgBonus+= ElementalAura->DamageTakenBonus;
        DefPen  += ElementalAura->DefensePenalty;
        bImmob  |= ElementalAura->bIsImmobilized;
    }

    for (const auto& E : ActiveEffects)
    {
        Spd      += E->GetSpeedPenalty();
        DmgBonus += E->GetDamageTakenBonus();
        DefPen   += E->GetDefensePenalty();
        AtkBonus += E->GetAttackBonus();
        DefBonus += E->GetDefenseBonus();
        AtkPen   += E->GetAttackPenalty();
        AtkSpdPen+= E->GetAttackSpeedPenalty();
        LuckPen  += E->GetLuckPenalty();
        MinePen  += E->GetMiningSpeedPenalty();
        bImmob   |= E->GetImmobilizes();
        bNoSkills|= E->GetCannotCastSkills();
        bNoMP    |= E->GetCannotUseMP();
        bSuperArmor |= E->GetHasSuperArmor();
        bEthereal   |= E->GetIsEthereal();
        bInvinc     |= E->GetIsInvincible();
        bBasicRes   |= E->GetHasBasicElemResistance();
        bAdvRes     |= E->GetHasAdvancedElemResistance();
    }

    // 霸體覆蓋所有行動/技能封鎖
    if (bSuperArmor)
    {
        bImmob    = false;
        bNoSkills = false;
    }

    TotalSpeedPenalty       = Spd;
    TotalDamageTakenBonus   = DmgBonus;
    TotalDefensePenalty     = FMath::Min(1.f, DefPen);
    TotalAttackBonus        = AtkBonus;
    TotalDefenseBonus       = DefBonus;
    TotalAttackPenalty      = AtkPen;
    TotalAttackSpeedPenalty = AtkSpdPen;
    TotalLuckPenalty        = LuckPen;
    TotalMiningSpeedPenalty = FMath::Min(0.9f, MinePen);
    bIsImmobilized          = bImmob;
    bCannotCastSkills       = bNoSkills;
    bCannotUseMP            = bNoMP;
    bHasSuperArmor          = bSuperArmor;
    bIsEthereal             = bEthereal;
    bIsInvincible           = bInvinc;
    bHasBasicElemResistance = bBasicRes;
    bHasAdvElemResistance   = bAdvRes;
}
