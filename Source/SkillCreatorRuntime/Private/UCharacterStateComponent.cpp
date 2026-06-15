#include "UCharacterStateComponent.h"

UCharacterStateComponent::UCharacterStateComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

float UCharacterStateComponent::TickState(float DeltaTime, bool bInCombat)
{
    if (bInCombat)
    {
        DrainStamina(StaminaDrainCombat * DeltaTime);
        DrainMentalEnergy(MentalEnergyDrainCombat * DeltaTime);
    }
    else
    {
        RestoreStamina(StaminaRegenPerSec * DeltaTime);
        RestoreMentalEnergy(MentalEnergyRegenPerSec * DeltaTime);
    }

    if (Mood < MoodDefault)
        ModifyMood(1.f * DeltaTime);

    // M-7 在此加入口渴 / 飢餓 / 氧氣 / 體溫傷害計算
    return 0.f;
}
