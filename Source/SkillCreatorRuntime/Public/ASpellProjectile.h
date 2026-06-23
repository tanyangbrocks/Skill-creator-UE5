#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridPos.h"
#include "ASpellProjectile.generated.h"

class AEnemyManager;
class AVoxelWorldActor;
class AEnemy;
class ASkillCreatorCharacter;
struct FCharacterStats;

// 離散 tile 移動的技能投射物。
// 每 MoveInterval 秒移動一格；命中敵人或實心 tile 後呼叫回調並銷毀。
// M-5：直接傷害回調；M-9 接通 VM ContainerEffect。
UCLASS()
class SKILLCREATORRUNTIME_API ASpellProjectile : public AActor
{
    GENERATED_BODY()
public:
    ASpellProjectile();

    // 初始化（由 USpellCaster 在 Spawn 後立即呼叫）
    // InDir：浮點方向向量（不需預先正規化；Init 內部自動 Normalize）
    void Init(FGridPos InOrigin, FVector InDir,
              AEnemyManager* InEnemyMgr, AVoxelWorldActor* InVoxelWorld);

    // ── 可調參數 ──────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, Category="Projectile")
    float BaseDamage    = 10.f;

    UPROPERTY(EditAnywhere, Category="Projectile")
    float MoveInterval  = 0.08f;   // 秒/格

    UPROPERTY(EditAnywhere, Category="Projectile")
    int32 MaxRange      = 20;       // 最大飛行格數

    // 命中敵人回調（由 USpellCaster 設定，玩家技能用）
    TFunction<void(AEnemy*, FGridPos)> OnHitEnemy;

    // 敵人投射物：設定後 AdvanceOneTile 在命中玩家 tile 時走 B-3 傷害管線
    // 玩家技能投射物保持 nullptr（不傷玩家）
    TWeakObjectPtr<ASkillCreatorCharacter> PlayerTarget;
    const FCharacterStats* AttackerStats = nullptr;

    bool IsAlive() const { return IsValid(this); }

    virtual void Tick(float DeltaTime) override;

private:
    FGridPos    CurrentPos;
    FVector     TileDir;          // 正規化浮點方向向量（支援對角線/任意角度投射）
    FVector     FloatPos;         // 累積浮點位置；整數部份決定下一個目標 tile
    int32       TilesTravelled = 0;
    float       MoveTimer      = 0.f;

    UPROPERTY() TObjectPtr<AEnemyManager>    EnemyMgr;
    UPROPERTY() TObjectPtr<AVoxelWorldActor> VoxelWorld;

    // 嘗試移動一格；碰牆或越界時銷毀；命中敵人時呼叫 OnHitEnemy 後銷毀。
    void AdvanceOneTile();

    AEnemy* FindEnemyAt(const FGridPos& Pos) const;
};
