#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ICreature.h"
#include "IElementalTarget.h"
#include "ISnapshottable.h"
#include "GridPos.h"
#include "SnapshotTypes.h"
#include "ActionBus.h"
#include "CharacterStats.h"
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

    // ── 完整數值（B-系列，所有生物共用 stats 管線）──────────────
    // 預設值在 AEnemy::BeginPlay() 依 Type 設定
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats")
    FCharacterStats Stats;

    // ── 組件 ──────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UElementalAuraComponent> AuraComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<AVoxelWorldActor> CachedVoxelWorld;

    // 2026-06-22 修復：純 C++ AEnemy（及其 BP_Enemy 子類，僅多覆寫 AIControllerClass）
    // 從未建立任何網格元件——對應 Godot 那邊敵人視覺其實是 Main.cs:1716 CreateEnemyMesh()
    // 外部建立的 MeshInstance3D（per-type 純色 BoxMesh，邊長 = Grain×TileSize，
    // SyncEnemyMeshes() 每幀同步位置），Enemy.cs 本體完全沒有視覺元件。UE5 這邊 AEnemy
    // 本身是 APawn，比照 ASkillCreatorCharacter（ACharacter 內建 Mesh）的作法，直接在
    // actor 內建一個 StaticMeshComponent 當身體，省去額外的外部 dictionary 同步邏輯——
    // SetActorLocation() 既有呼叫點不變，掛在 root 上的 mesh 自動跟著移動。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<class UStaticMeshComponent> MeshComp;

    // 傷害攔截管線（對應 Godot Enemy.cs:425 ActionBus.Dispatch(EntityDamageAction)）。
    // Godot 端 ActionBus 是全域靜態，玩家/敵人共用一條管線；UE5 維持既有的 per-instance
    // 設計（同 ASkillCreatorCharacter::ActionBus），讓未來「對敵人的傷害護盾」類效果可註冊。
    // 2026-06-20 Round3 C-6 修正：原本敵人傷害完全沒有走攔截管線。
    FActionBus ActionBus;

    // ── 戰鬥參數（依 Type 決定）──────────────────────────────────
    float GetBaseMoveInterval() const;
    float GetMoveInterval()     const;   // 乘上 Aura.SpeedPenalty
    float GetAttackInterval()   const;
    float GetAttackDamage()     const;
    int32 GetAttackRange()      const;
    int32 GetDetectRange()      const;
    float GetXpReward()         const;

    // ── 傷害管線（B-3，與 ASkillCreatorCharacter 同一套公式）────
    // 包含：命中/閃避 → 防禦/減傷 → 暴擊判定 → TakeDamageAmount
    // AttackerStats = nullptr 時跳過暴擊/閃避（純防禦計算）
    void TakePhysicalDamage(float PhysAtk, const FCharacterStats* AttackerStats = nullptr);
    void TakeEnergyDamage(float EnergyAtk, FName ManaTypeKey, const FCharacterStats* AttackerStats = nullptr);

    // ── 生命週期 ──────────────────────────────────────────────────
    void TakeDamageAmount(float Amount);
    void ForceDespawn() { Hp = 0.f; }
    void Respawn();
    void StartRespawn(float DelaySeconds = 8.f);
    void TickRespawn(float /*DeltaTime*/) {}  // 保留 API；計時器由 FTimerHandle 驅動
    void ForceRevive();
    void ApplyGravity();

    // 依目前 Type 重新上色（AEnemyManager::Spawn 在 SpawnActor 之後才設定 Type，
    // 此時 BeginPlay 已經跑過一次預設色，需要再呼叫一次套用正確顏色）
    void ApplyBodyColor();

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

public:
    // I-9：Patrol 巡邏方向（對應 Godot Enemy.cs:50 _patrolDir，±1 沿 X 軸來回）
    int32 PatrolDir = 1;
};
