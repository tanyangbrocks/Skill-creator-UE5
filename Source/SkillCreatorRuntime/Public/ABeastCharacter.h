#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ICreature.h"
#include "IElementalTarget.h"
#include "ISnapshottable.h"
#include "ICombatant.h"
#include "GridPos.h"
#include "SnapshotTypes.h"
#include "ActionBus.h"
#include "CharacterStats.h"
#include "AttackTypes.h"
#include "CreatureTypes.h"
#include "ABeastCharacter.generated.h"

class UElementalAuraComponent;
class USpecialStatusComponent;
class AVoxelWorldActor;
class ASkillCreatorCharacter;

// 戰鬥型態（魔獸底下目前定義的四種，不同 EBeastKind 可定義自己的型態集合）
UENUM(BlueprintType)
enum class EEnemyType : uint8
{
    Melee   UMETA(DisplayName="近戰"),
    Ranged  UMETA(DisplayName="遠程"),
    Patrol  UMETA(DisplayName="巡邏"),
    Heavy   UMETA(DisplayName="重裝"),
};

UENUM(BlueprintType)
enum class EEnemyState : uint8
{
    Idle,
    Chase,
    Attack,
};

// 生成分類（Common=通用敵人；Area=區域敵人；Named/Boss=特殊）
// 概念上對應「敵人標籤底下的細分」，與 ECreatureKind 正交
UENUM(BlueprintType)
enum class ESpawnCategory : uint8
{
    Common  UMETA(DisplayName="通用"),
    Area    UMETA(DisplayName="區域"),
    Named   UMETA(DisplayName="命名"),
    Boss    UMETA(DisplayName="Boss"),
};

// 「獸」類生物的基底類別（對應舊 AEnemy，現在是完整的生物概念）。
// ECreatureKind = Beast；EBeastKind 決定白/魔/靈/妖；EHostility 決定是否帶「敵人」標籤。
// 現有所有戰鬥型態（Melee/Ranged/Patrol/Heavy）先歸類為 EBeastKind::Magic（魔獸），
// 未來可透過切換 BeastKind 擴充白獸、靈獸、妖獸分支。
UCLASS()
class SKILLCREATORRUNTIME_API ABeastCharacter
    : public APawn
    , public ICreature
    , public ICombatant
    , public IElementalTarget
    , public ISnapshottable
{
    GENERATED_BODY()
public:
    ABeastCharacter();

    // ── 種類標籤 ──────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Creature")
    EBeastKind BeastKind = EBeastKind::Magic;

    // 敵人標籤（動態：可在 Neutral ↔ Hostile 間切換）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Creature")
    EHostility Hostility = EHostility::Hostile;

    bool IsEnemy() const { return Hostility == EHostility::Hostile; }
    virtual ECreatureKind GetCreatureKind() const override { return ECreatureKind::Beast; }

    // ── 戰鬥型態 ──────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beast")
    EEnemyType  Type     = EEnemyType::Melee;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beast")
    ESpawnCategory Category = ESpawnCategory::Common;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beast")
    float Hp    = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beast")
    float MaxHp = 50.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beast")
    FGridPos GridPosition;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beast")
    FGridPos SpawnGridPos;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beast")
    EEnemyState AIState = EEnemyState::Idle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beast")
    uint8 WandsToFire = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beast")
    int32 FacingX = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beast")
    int32 FacingZ = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats")
    FCharacterStats Stats;

    // ── 組件 ──────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UElementalAuraComponent> AuraComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<USpecialStatusComponent> SpecialStatusComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<AVoxelWorldActor> CachedVoxelWorld;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<class UStaticMeshComponent> MeshComp;

    FActionBus ActionBus;

    // ── 戰鬥參數 ──────────────────────────────────────────────────
    float GetBaseMoveInterval() const;
    float GetMoveInterval()     const;
    float GetAttackInterval()   const;
    float GetAttackDamage()     const;
    int32 GetAttackRange()      const;
    int32 GetDetectRange()      const;
    float GetXpReward()         const;

    // ── ICombatant ────────────────────────────────────────────────
    virtual FCharacterStats&       GetStats()       override { return Stats; }
    virtual const FCharacterStats& GetStats() const override { return Stats; }
    virtual bool                   IsHostile()      const override { return Hostility == EHostility::Hostile; }
    virtual bool                   OccupiesTile(FGridPos Pos) const override;
    virtual UElementalAuraComponent* GetAuraComp()  const override;
    virtual AActor*                AsActor()               override { return this; }
    virtual IElementalTarget*      AsElementalTarget()     override { return this; }
    virtual void                   ApplyFinalDamage(float FinalDmg) override { TakeDamageAmount(FinalDmg); }
    virtual float GetStatusDefensePenalty()   const override;
    virtual float GetStatusDamageTakenBonus() const override;
    virtual bool  IsInvincible()              const override;
    virtual float GetStatusAttackPenalty()    const override;
    virtual bool  HasBasicElemResistance()    const override;

    // ── 傷害管線（B-3；ICombatant override，FCombatResolver 統一公式）──
    virtual void TakePhysicalDamage(float PhysAtk, const FCharacterStats* Atk = nullptr, AActor* Attacker = nullptr) override;
    virtual void TakeEnergyDamage(float EnergyAtk, FName ManaTypeKey, const FCharacterStats* Atk = nullptr) override;
    virtual void TakeElementalDamage(float ElemAtk, ESkillElementType Element, bool bEnergyDefenseApplies = false, const FCharacterStats* Atk = nullptr) override;
    virtual void ApplyElementalAuraImmediate(ESkillElementType Elem, float Duration) override;

    // ── 生命週期 ──────────────────────────────────────────────────
    void TakeDamageAmount(float Amount);
    void ForceDespawn() { Hp = 0.f; }
    void Respawn();
    void StartRespawn(float DelaySeconds = 8.f);
    void TickRespawn(float /*DeltaTime*/) {}
    void ForceRevive();
    void ApplyGravity();
    void ApplyBodyColor();

    // ── S-2 攻擊框架（Melee）───────────────────────────────────
    EAttackPhase AttackPhase = EAttackPhase::None;
    void BeginMeleeAttack(ASkillCreatorCharacter* Target);

    // ── ISnapshottable ────────────────────────────────────────────
    virtual FEntitySnapshot TakeSnapshot()                              const override;
    virtual void            RestoreFromSnapshot(const FEntitySnapshot&)       override;

    // ── ICreature + ICombatant（同名 override 同時滿足兩介面）───────
    virtual int32    GetCreatureId() const override { return UniqueId; }
    virtual FGridPos GetPosition()   const override { return GridPosition; }
    virtual float    GetHp()         const override { return Hp; }
    virtual float    GetMaxHp()      const override { return MaxHp; }
    virtual bool     IsAlive()       const override { return Hp > 0.f; }

    // ── IElementalTarget ──────────────────────────────────────────
    virtual int32 GetEntityId()              const override { return UniqueId; }
    virtual void  TakeDirectDamage(float Amount)   override { TakeDamageAmount(Amount); }

    // I-9：Patrol 巡邏方向
    int32 PatrolDir = 1;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
    int32         UniqueId        = 0;
    static int32  NextId;
    bool          bPendingRespawn = false;
    FTimerHandle  RespawnTimerHandle;

    TWeakObjectPtr<ASkillCreatorCharacter> MeleeTarget;
    FTimerHandle WindupTimer;
    FTimerHandle ActiveTimer;
    FTimerHandle RecoveryTimer;
    void OnWindupEnd();
    void OnActiveEnd();
    void OnRecoveryEnd();
};
