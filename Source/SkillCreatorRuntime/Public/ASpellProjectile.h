#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridPos.h"
#include "ASpellProjectile.generated.h"

class AEnemyManager;
class AVoxelWorldActor;
class AEnemy;

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
    void Init(FGridPos InOrigin, FIntVector InDir,
              AEnemyManager* InEnemyMgr, AVoxelWorldActor* InVoxelWorld);

    // ── 可調參數 ──────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, Category="Projectile")
    float BaseDamage    = 10.f;

    UPROPERTY(EditAnywhere, Category="Projectile")
    float MoveInterval  = 0.08f;   // 秒/格

    UPROPERTY(EditAnywhere, Category="Projectile")
    int32 MaxRange      = 20;       // 最大飛行格數

    // 命中敵人回調（由 USpellCaster 設定）
    TFunction<void(AEnemy*, FGridPos)> OnHitEnemy;

    virtual void Tick(float DeltaTime) override;

private:
    FGridPos    CurrentPos;
    FIntVector  TileDir;
    int32       TilesTravelled = 0;
    float       MoveTimer      = 0.f;

    UPROPERTY() TObjectPtr<AEnemyManager>    EnemyMgr;
    UPROPERTY() TObjectPtr<AVoxelWorldActor> VoxelWorld;

    // 嘗試移動一格；碰牆或越界時銷毀；命中敵人時呼叫 OnHitEnemy 後銷毀。
    void AdvanceOneTile();

    AEnemy* FindEnemyAt(const FGridPos& Pos) const;
};
