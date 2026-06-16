#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ICreature.h"
#include "IElementalTarget.h"
#include "ISnapshottable.h"
#include "GridPos.h"
#include "SnapshotTypes.h"
#include "AEnemy.generated.h"

class UElementalAuraComponent;
class AVoxelWorldActor;

UENUM(BlueprintType)
enum class EEnemyType : uint8
{
    Melee   UMETA(DisplayName="近戰"),   // 追擊+近距攻擊
    Ranged  UMETA(DisplayName="遠程"),   // 維持距離+投射物
    Patrol  UMETA(DisplayName="巡邏"),   // 固定路線，玩家接近才追
    Heavy   UMETA(DisplayName="重裝"),   // 高 HP、緩慢、重擊
};

UENUM(BlueprintType)
enum class EEnemyState : uint8
{
    Idle,
    Chase,
    Attack,
};

UENUM(BlueprintType)
enum class ESpawnCategory : uint8
{
    Common  UMETA(DisplayName="一般"),
    Area    UMETA(DisplayName="區域"),
    Named   UMETA(DisplayName="命名"),
    Boss    UMETA(DisplayName="Boss"),
};

// 敵人 Pawn（對應 Godot Enemy.cs）。
// 採離散 tile 移動（APawn，不使用 CharacterMovementComponent）。
// AI 邏輯由 AEnemyAIController + BehaviorTree 驅動（BT asset → M-8）。
// A* tile 移動 → M-5；投射物 → M-5。
UCLASS()
class SKILLCREATORRUNTIME_API AEnemy
    : public APawn
    , public ICreature
    , public IElementalTarget
    , public ISnapshottable
{
    GENERATED_BODY()
public:
    AEnemy();

    // ── 基礎屬性 ──────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy")
    EEnemyType  Type     = EEnemyType::Melee;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy")
    ESpawnCategory Category = ESpawnCategory::Common;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy")
    float Hp    = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy")
    float MaxHp = 50.f;

    // 格座標位置（tile-space）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy")
    FGridPos GridPosition;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy")
    FGridPos SpawnGridPos;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy")
    EEnemyState AIState = EEnemyState::Idle;

    // 遠程敵人使用的魔杖槽數
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy")
    uint8 WandsToFire = 1;

    // 面向方向（tile 空間；各軸 -1/0/+1）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy")
    int32 FacingX = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy")
    int32 FacingZ = 0;

    // ── 組件 ──────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UElementalAuraComponent> AuraComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<AVoxelWorldActor> CachedVoxelWorld;

    // ── 戰鬥參數（依 Type 決定）──────────────────────────────────
    float GetBaseMoveInterval() const;
    float GetMoveInterval()     const;   // 乘上 Aura.SpeedPenalty
    float GetAttackInterval()   const;
    float GetAttackDamage()     const;
    int32 GetAttackRange()      const;
    int32 GetDetectRange()      const;
    float GetXpReward()         const;

    // ── 生命週期 ──────────────────────────────────────────────────
    void TakeDamageAmount(float Amount);
    void ForceDespawn() { Hp = 0.f; }
    void Respawn();
    void StartRespawn(float DelaySeconds = 5.f);
    void TickRespawn(float /*DeltaTime*/) {}  // 保留 API；計時器由 FTimerHandle 驅動
    void ForceRevive();
    void ApplyGravity();

    // ── ISnapshottable ────────────────────────────────────────────
    virtual FEntitySnapshot TakeSnapshot()                              const override;
    virtual void            RestoreFromSnapshot(const FEntitySnapshot&)       override;

    // ── ICreature ─────────────────────────────────────────────────
    virtual int32    GetCreatureId() const override { return UniqueId; }
    virtual FGridPos GetPosition()   const override { return GridPosition; }
    virtual float    GetHp()         const override { return Hp; }
    virtual float    GetMaxHp()      const override { return MaxHp; }
    virtual bool     IsAlive()       const override { return Hp > 0.f; }

    // ── IElementalTarget ──────────────────────────────────────────
    virtual int32 GetEntityId()              const override { return UniqueId; }
    virtual void  TakeDirectDamage(float Amount)   override { TakeDamageAmount(Amount); }

protected:
    virtual void BeginPlay() override;

private:
    int32         UniqueId        = 0;
    static int32  NextId;
    bool          bPendingRespawn = false;
    FTimerHandle  RespawnTimerHandle;
};
