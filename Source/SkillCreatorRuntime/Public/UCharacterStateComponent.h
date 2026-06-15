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

// 位元旗標（同時可有多種狀態）
UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class ESocialStatus : uint8
{
    Normal   = 0,
    Wanted   = 1 << 0,  // 被通緝
    Banned   = 1 << 1,  // 禁止出入
    Welcomed = 1 << 2,  // 被歡迎
};
ENUM_CLASS_FLAGS(ESocialStatus)

// 角色動態狀態（對應 Godot CharacterState.cs W-5b）。
// M-4 骨架：只啟用 Stamina / MentalEnergy / Mood；
// 完整生存系統（體溫、口渴、飢餓、氧氣）→ M-7 填入。
UCLASS(ClassGroup=SkillCreator, meta=(BlueprintSpawnableComponent))
class SKILLCREATORRUNTIME_API UCharacterStateComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCharacterStateComponent();

    // ── 體力（Stamina）────────────────────────────────────────────
    static constexpr float MaxStamina         = 100.f;
    static constexpr float StaminaRegenPerSec = 5.f;
    static constexpr float StaminaDrainCombat = 2.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State")
    float Stamina = MaxStamina;
    bool  IsStaminaDepleted() const { return Stamina <= 1.f; }

    void DrainStamina(float V)   { Stamina = FMath::Max(0.f, Stamina - V); }
    void RestoreStamina(float V) { Stamina = FMath::Min(MaxStamina, Stamina + V); }

    // ── 精力（MentalEnergy）──────────────────────────────────────
    static constexpr float MaxMentalEnergy         = 100.f;
    static constexpr float MentalEnergyRegenPerSec = 3.f;
    static constexpr float MentalEnergyDrainCombat = 1.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State")
    float MentalEnergy = MaxMentalEnergy;
    bool  IsMentalEnergyDepleted() const { return MentalEnergy <= 1.f; }

    void DrainMentalEnergy(float V)   { MentalEnergy = FMath::Max(0.f, MentalEnergy - V); }
    void RestoreMentalEnergy(float V) { MentalEnergy = FMath::Min(MaxMentalEnergy, MentalEnergy + V); }

    // ── 心情（Mood）──────────────────────────────────────────────
    static constexpr float MaxMood               = 100.f;
    static constexpr float MoodInsanityThreshold = 10.f;
    static constexpr float MoodDefault           = 70.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State")
    float Mood = MoodDefault;
    bool  IsInsane() const { return Mood <= MoodInsanityThreshold; }

    void ModifyMood(float D) { Mood = FMath::Clamp(Mood + D, 0.f, MaxMood); }

    // ── 健康狀態 / 社會狀態（stub，M-7 接入邏輯）────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State")
    EHealthCondition HealthStatus = EHealthCondition::Healthy;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="State")
    ESocialStatus Social = ESocialStatus::Normal;

    // ── 每幀更新（傳入是否戰鬥中）────────────────────────────────
    // 回傳本幀應直接扣除的生存傷害（M-7 生存系統接入後非零）
    float TickState(float DeltaTime, bool bInCombat);

    // TODO M-7: 體溫、口渴、飢餓、氧氣欄位及完整 Tick 邏輯
};
