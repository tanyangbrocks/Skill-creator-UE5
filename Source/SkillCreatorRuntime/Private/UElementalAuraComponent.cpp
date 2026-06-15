#include "UElementalAuraComponent.h"

UElementalAuraComponent::UElementalAuraComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UElementalAuraComponent::Process(float DeltaTime)
{
    for (int32 i = Auras.Num() - 1; i >= 0; --i)
    {
        Auras[i].Duration -= DeltaTime;
        if (Auras[i].Duration <= 0.f)
            Auras.RemoveAt(i);
    }
    RecalcAggregates();
}

void UElementalAuraComponent::Reset()
{
    Auras.Reset();
    RecalcAggregates();
}

void UElementalAuraComponent::RecalcAggregates()
{
    // M-5 填入：依各 Aura 元素類型累加 SpeedPenalty / DamageTakenBonus 等
    SpeedPenalty         = 0.f;
    bIsImmobilized       = false;
    DamageTakenBonus     = 0.f;
    DefensePenalty       = 0.f;
    AuraTemperatureShift = 0.f;
}
