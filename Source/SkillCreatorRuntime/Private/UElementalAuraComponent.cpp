#include "UElementalAuraComponent.h"
#include "ElementalReactionTable.h"

UElementalAuraComponent::UElementalAuraComponent()
{
    PrimaryComponentTick.bCanEverTick = true;  // Bug 1 fix: 自帶 tick，任何持有本元件的 Actor 均自動更新
}

void UElementalAuraComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    Process(DeltaTime);
}

// ── 主要 API ──────────────────────────────────────────────────────────────

bool UElementalAuraComponent::Apply(ESkillElementType Element, float Duration, IElementalTarget* Target)
{
    if (float* Cd = CdLeft.Find(Element); Cd && *Cd > 0.f) return false;
    CdLeft.Add(Element, ApplicationCooldownSec);
    ApplyInternal(Element, Duration, Target);
    return true;
}

void UElementalAuraComponent::ApplyImmediate(ESkillElementType Element, float Duration, IElementalTarget* Target)
{
    ApplyInternal(Element, Duration, Target);
}

void UElementalAuraComponent::ApplyFreeze(float Duration, IElementalTarget* Target)
{
    auto Fx = MakeUnique<FFrozenEffect>();
    Fx->RemainingDuration = Duration;
    AddEffect(MoveTemp(Fx), Target);
}

void UElementalAuraComponent::ApplySlow(float Duration, IElementalTarget* Target)
{
    auto Fx = MakeUnique<FGrowthSlowEffect>();
    Fx->RemainingDuration = Duration;
    AddEffect(MoveTemp(Fx), Target);
}

// ── 每幀更新 ──────────────────────────────────────────────────────────────

void UElementalAuraComponent::Process(float DeltaTime)
{
    // 1. 應用冷卻計時
    for (auto It = CdLeft.CreateIterator(); It; ++It)
    {
        It.Value() -= DeltaTime;
        if (It.Value() <= 0.f) It.RemoveCurrent();
    }

    // 2. Aura 存在計時（倒計時到 0 自動移除）
    for (int32 i = Auras.Num() - 1; i >= 0; --i)
    {
        Auras[i].Duration -= DeltaTime;
        if (Auras[i].Duration <= 0.f) Auras.RemoveAt(i);
    }

    // 3. 效果計時 + 每幀 tick
    for (int32 i = ActiveEffects.Num() - 1; i >= 0; --i)
    {
        ActiveEffects[i]->OnProcess(DeltaTime);
        ActiveEffects[i]->RemainingDuration -= DeltaTime;
        if (ActiveEffects[i]->RemainingDuration <= 0.f)
            ActiveEffects.RemoveAt(i);
    }

    RecalcAggregates();
}

void UElementalAuraComponent::Reset()
{
    Auras.Reset();
    ActiveEffects.Reset();
    CdLeft.Reset();
    RecalcAggregates();
}

// ── 內部輔助 ──────────────────────────────────────────────────────────────

void UElementalAuraComponent::ApplyInternal(ESkillElementType Element, float Duration, IElementalTarget* Target)
{
    // 掃描現有 Aura，找第一個可配對的反應
    for (int32 i = 0; i < Auras.Num(); ++i)
    {
        ESkillElementType Existing = Auras[i].Element;
        const FReactionDef* Def = FElementalReactionTable::Lookup(Element, Existing);
        if (!Def) continue;

        // 找到反應：消耗現有 Aura
        Auras.RemoveAt(i);
        UE_LOG(LogTemp, Log, TEXT("[Elemental] %d + %d → %s"),
               (int32)Existing, (int32)Element, *Def->Name);

        // 套用元素狀態效果（若有）
        if (Def->MakeStatusEffect)
            AddEffect(Def->MakeStatusEffect(), Target);

        // incoming element 已用於反應，不加入 Aura 清單
        return;
    }

    // 與 NativeElement 配對（NativeElement 永久存在，不被消耗）
    if (NativeElement != ESkillElementType::None)
    {
        const FReactionDef* Def = FElementalReactionTable::Lookup(Element, NativeElement);
        if (Def)
        {
            UE_LOG(LogTemp, Log, TEXT("[Elemental] NativeElem %d + incoming %d => %s"),
                   (int32)NativeElement, (int32)Element, *Def->Name);
            if (Def->MakeStatusEffect)
                AddEffect(Def->MakeStatusEffect(), Target);
            return;  // incoming element 已被 NativeElement 消耗，不入 aura 池
        }
    }

    // 無反應：加入 Aura 清單，等待未來配對
    Auras.Add({ Element, Duration });
}

void UElementalAuraComponent::AddEffect(TUniquePtr<FElementalStatusEffect> Fx, IElementalTarget* Target)
{
    const FName TypeId    = Fx->GetEffectType();
    const int32 MaxStacks = Fx->GetMaxStacks();

    int32 Count = 0;
    for (const TUniquePtr<FElementalStatusEffect>& E : ActiveEffects)
        if (E->GetEffectType() == TypeId) ++Count;

    if (Count >= MaxStacks) return;

    Fx->OnApply(Target);
    ActiveEffects.Add(MoveTemp(Fx));
    RecalcAggregates();
}

// ── 快照 API ──────────────────────────────────────────────────────────────

FAuraSnapshot UElementalAuraComponent::TakeAuraSnapshot() const
{
    FAuraSnapshot Snap;
    for (const FAuraEntry& A : Auras)
    {
        FAuraEntryData D;
        D.Element  = A.Element;
        D.Duration = A.Duration;
        Snap.Entries.Add(D);
    }
    for (const TUniquePtr<FElementalStatusEffect>& Fx : ActiveEffects)
    {
        FAuraEffectData D;
        D.EffectType        = Fx->GetEffectType();
        D.RemainingDuration = Fx->RemainingDuration;
        D.AccumulatedState  = Fx->GetAccumulatedState();
        Snap.Effects.Add(D);
    }
    return Snap;
}

void UElementalAuraComponent::RestoreAuraSnapshot(const FAuraSnapshot& Snap)
{
    Reset();
    for (const FAuraEntryData& D : Snap.Entries)
        Auras.Add({ D.Element, D.Duration });
    for (const FAuraEffectData& D : Snap.Effects)
    {
        if (TUniquePtr<FElementalStatusEffect> Fx = CreateEffect(D))
            ActiveEffects.Add(MoveTemp(Fx));
    }
    RecalcAggregates();
}

TUniquePtr<FElementalStatusEffect> UElementalAuraComponent::CreateEffect(const FAuraEffectData& Data)
{
    TUniquePtr<FElementalStatusEffect> Fx;
    if      (Data.EffectType == "Rust")          Fx = MakeUnique<FRustEffect>();
    else if (Data.EffectType == "GrowthSlow")    Fx = MakeUnique<FGrowthSlowEffect>();
    else if (Data.EffectType == "Quicksand")     Fx = MakeUnique<FQuicksandSlowEffect>();
    else if (Data.EffectType == "Electrocution") Fx = MakeUnique<FElectrocutionEffect>();
    else if (Data.EffectType == "Frozen")        Fx = MakeUnique<FFrozenEffect>();
    if (Fx)
    {
        Fx->RemainingDuration = Data.RemainingDuration;
        Fx->RestoreAccumulatedState(Data.AccumulatedState);
    }
    return Fx;
}

void UElementalAuraComponent::RecalcAggregates()
{
    float Spd = 0.f, DmgBonus = 0.f, DefPen = 0.f, TempShift = 0.f;
    bool bImmob = false;

    for (const TUniquePtr<FElementalStatusEffect>& E : ActiveEffects)
    {
        Spd      += E->GetSpeedPenalty();
        DmgBonus += E->GetDamageTakenBonus();
        DefPen   += E->GetDefensePenalty();
        bImmob   |= E->GetImmobilizes();
    }

    for (const FAuraEntry& A : Auras)
    {
        switch (A.Element)
        {
            case ESkillElementType::Fire:  TempShift += FireAuraTempShift;  break;
            case ESkillElementType::Ice:   TempShift += IceAuraTempShift;   break;
            case ESkillElementType::Water: TempShift += WaterAuraTempShift; break;
            default: break;
        }
    }

    SpeedPenalty         = Spd;
    DamageTakenBonus     = DmgBonus;
    DefensePenalty       = FMath::Min(1.f, DefPen);
    bIsImmobilized       = bImmob;
    AuraTemperatureShift = FMath::Clamp(TempShift, -AuraTempShiftMax, AuraTempShiftMax);
}
