#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UCharacterStateComponent.generated.h"

UENUM(BlueprintType)
enum class EHealthCondition : uint8
{
    Healthy   UMETA(DisplayName="健康"),
    Weakened  UMETA(DisplayName="衰弱"),
    Insomnia  UMETA(DisplayName="失眠"),
    HeavyCold UMETA(DisplayName="重感冒"),
};

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class ESocialStatus : uint8
{
    Normal   = 0,
    Wanted   = 1 << 0,
    Banned   = 1 << 1,
    Welcomed = 1 << 2,
};
ENUM_CLASS_FLAGS(ESocialStatus)

// 角色動態狀態（對應 Godot CharacterState.cs W-5b）。
// Tick() 回傳本幀應直接扣除的生存傷害總量（繞過防禦管線）。
UCLASS(ClassGroup=SkillCreator, meta=(BlueprintSpawnableComponent))
class SKILLCREATORRUNTIME_API UCharacterStateComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCharacterStateComponent();

    // ════════════════════════════════════════════════════════════════
    //  體力（Stamina）
    // ════════════════════════════════════════════════════════════════
    static constexpr float MaxStamina               = 100.f;
    static constexpr float StaminaRegenPerSec       = 5.f;
    static constexpr float StaminaDrainCombat       = 2.f;
    static constexpr float StaminaDepletedThreshold = 1.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State|Stamina")
    float Stamina = MaxStamina;
    bool IsStaminaDepleted() const { return Stamina <= StaminaDepletedThreshold; }

    void DrainStamina(float V)    { Stamina = FMath::Max(0.f, Stamina - V); }
    void RestoreStamina(float V)  { Stamina = FMath::Min(MaxStamina, Stamina + V); }
    void SetStamina(float V)      { Stamina = FMath::Clamp(V, 0.f, MaxStamina); }

    // ════════════════════════════════════════════════════════════════
    //  精力（MentalEnergy）
    // ════════════════════════════════════════════════════════════════
    static constexpr float MaxMentalEnergy         = 100.f;
    static constexpr float MentalEnergyRegenPerSec = 3.f;
    static constexpr float MentalEnergyDrainCombat = 1.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State|MentalEnergy")
    float MentalEnergy = MaxMentalEnergy;
    bool IsMentalEnergyDepleted() const { return MentalEnergy <= 1.f; }

    void DrainMentalEnergy(float V)   { MentalEnergy = FMath::Max(0.f, MentalEnergy - V); }
    void RestoreMentalEnergy(float V) { MentalEnergy = FMath::Min(MaxMentalEnergy, MentalEnergy + V); }
    void SetMentalEnergy(float V)     { MentalEnergy = FMath::Clamp(V, 0.f, MaxMentalEnergy); }

    // ════════════════════════════════════════════════════════════════
    //  心情（Mood）
    // ════════════════════════════════════════════════════════════════
    static constexpr float MaxMood               = 100.f;
    static constexpr float MoodInsanityThreshold = 10.f;
    static constexpr float MoodDefault           = 70.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State|Mood")
    float Mood = MoodDefault;
    bool IsInsane() const { return Mood <= MoodInsanityThreshold; }

    void ModifyMood(float D) { Mood = FMath::Clamp(Mood + D, 0.f, MaxMood); }
    void SetMood(float V)    { Mood = FMath::Clamp(V, 0.f, MaxMood); }
    void SetHealthStatus(EHealthCondition C) { HealthStatus = C; }

    // ════════════════════════════════════════════════════════════════
    //  體溫（BodyTemperature）
    //  向 AmbientTemperature 緩慢漂移；AmbientTemperature 由外部每幀設置
    // ════════════════════════════════════════════════════════════════
    static constexpr float NormalBodyTemp       = 36.5f;
    static constexpr float DefaultAmbientTemp   = 20.0f;
    static constexpr float HypothermiaThreshold = 10.0f;
    static constexpr float HeatstrokeThreshold  = 42.0f;
    static constexpr float BodyTempAdaptRate    = 1.5f;
    static constexpr float ThirstHeatMultiplier = 2.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State|Temperature")
    float BodyTemperature    = NormalBodyTemp;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="State|Temperature")
    float AmbientTemperature = DefaultAmbientTemp;

    bool IsHypothermic() const { return BodyTemperature <= HypothermiaThreshold; }
    bool IsHeatstroke()  const { return BodyTemperature >= HeatstrokeThreshold;  }

    // ════════════════════════════════════════════════════════════════
    //  口渴（Thirst）
    // ════════════════════════════════════════════════════════════════
    static constexpr float MaxThirst               = 100.f;
    static constexpr float ThirstDrainPerSec       = 0.05f;
    static constexpr float ThirstWarningThreshold  = 20.f;
    static constexpr float ThirstCriticalThreshold = 5.f;
    static constexpr float ThirstCriticalDamage    = 2.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State|Survival")
    float Thirst = MaxThirst;
    bool IsThirsty()    const { return Thirst <= ThirstWarningThreshold;  }
    bool IsDehydrated() const { return Thirst <= ThirstCriticalThreshold; }

    void DrainThirst(float V)   { Thirst = FMath::Max(0.f, Thirst - V); }
    void RestoreThirst(float V) { Thirst = FMath::Min(MaxThirst, Thirst + V); }

    // ════════════════════════════════════════════════════════════════
    //  飢餓（Hunger）
    // ════════════════════════════════════════════════════════════════
    static constexpr float MaxHunger               = 100.f;
    static constexpr float HungerDrainPerSec       = 0.02f;
    static constexpr float HungerWarningThreshold  = 20.f;
    static constexpr float HungerCriticalThreshold = 5.f;
    static constexpr float HungerCriticalDamage    = 1.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State|Survival")
    float Hunger = MaxHunger;
    bool IsHungry()   const { return Hunger <= HungerWarningThreshold;  }
    bool IsStarving() const { return Hunger <= HungerCriticalThreshold; }

    void DrainHunger(float V)   { Hunger = FMath::Max(0.f, Hunger - V); }
    void RestoreHunger(float V) { Hunger = FMath::Min(MaxHunger, Hunger + V); }

    // ════════════════════════════════════════════════════════════════
    //  氧氣（Oxygen）
    //  IsOxygenDeprived 由外部（PlayerController）每幀設置
    // ════════════════════════════════════════════════════════════════
    static constexpr float MaxOxygen               = 100.f;
    static constexpr float OxygenDrainPerSec       = 10.f;
    static constexpr float OxygenRegenPerSec       = 30.f;
    static constexpr float OxygenCriticalThreshold = 5.f;
    static constexpr float OxygenCriticalDamage    = 15.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State|Survival")
    float Oxygen = MaxOxygen;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="State|Survival")
    bool bIsOxygenDeprived = false;

    bool IsSuffocating() const { return Oxygen <= OxygenCriticalThreshold; }

    void DrainOxygen(float V)   { Oxygen = FMath::Max(0.f, Oxygen - V); }
    void RestoreOxygen(float V) { Oxygen = FMath::Min(MaxOxygen, Oxygen + V); }

    // ════════════════════════════════════════════════════════════════
    //  健康 / 社會狀態
    // ════════════════════════════════════════════════════════════════
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State")
    EHealthCondition HealthStatus = EHealthCondition::Healthy;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State")
    ESocialStatus Social = ESocialStatus::Normal;

    void AddSocialFlag(ESocialStatus F)    { Social |=  F; }
    void RemoveSocialFlag(ESocialStatus F) { Social &= ~F; }
    bool HasSocialFlag(ESocialStatus F) const { return EnumHasAnyFlags(Social, F); }

    // ════════════════════════════════════════════════════════════════
    //  每幀更新
    //  回傳本幀應直接扣除的生存傷害（繞過防禦管線）
    // ════════════════════════════════════════════════════════════════
    float TickState(float DeltaTime, bool bInCombat);
};
