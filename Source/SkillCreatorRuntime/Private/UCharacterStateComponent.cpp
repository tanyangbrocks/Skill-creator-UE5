#include "UCharacterStateComponent.h"

UCharacterStateComponent::UCharacterStateComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

float UCharacterStateComponent::TickState(float DeltaTime, bool bInCombat)
{
    float PendingDamage = 0.f;

    // ── 體力 / 精力 ─────────────────────────────────────────────────
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

    // ── 心情值緩慢回歸中值 ─────────────────────────────────────────
    if (Mood < MoodDefault)
        ModifyMood(1.f * DeltaTime);

    // ── 體溫：向 AmbientTemperature 漂移 ──────────────────────────
    {
        float TempDiff = AmbientTemperature - BodyTemperature;
        float Step     = BodyTempAdaptRate * DeltaTime;
        if (FMath::Abs(TempDiff) <= Step)
            BodyTemperature = AmbientTemperature;
        else
            BodyTemperature += FMath::Sign(TempDiff) * Step;
    }

    // ── 口渴：中暑時消耗加速 ──────────────────────────────────────
    {
        float Drain = ThirstDrainPerSec;
        if (IsHeatstroke()) Drain *= ThirstHeatMultiplier;
        DrainThirst(Drain * DeltaTime);
        if (IsDehydrated()) PendingDamage += ThirstCriticalDamage * DeltaTime;
    }

    // ── 飢餓 ──────────────────────────────────────────────────────
    DrainHunger(HungerDrainPerSec * DeltaTime);
    if (IsStarving()) PendingDamage += HungerCriticalDamage * DeltaTime;

    // ── 氧氣 ──────────────────────────────────────────────────────
    if (bIsOxygenDeprived)
    {
        DrainOxygen(OxygenDrainPerSec * DeltaTime);
        if (IsSuffocating()) PendingDamage += OxygenCriticalDamage * DeltaTime;
    }
    else
    {
        RestoreOxygen(OxygenRegenPerSec * DeltaTime);
    }

    return PendingDamage;
}
