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
    // F-3 審查（2026-06-20）：Godot CharacterState.cs:208 只在 Mood < Default 時回升，
    // 此「Mood > Default 也下降」為 UE5 刻意擴充（Godot 標記待完整設計），決定保留。
    if (Mood < MoodDefault)
        ModifyMood(MoodRecoveryRate * DeltaTime);
    else if (Mood > MoodDefault)
        ModifyMood(-MoodRecoveryRate * DeltaTime);

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
