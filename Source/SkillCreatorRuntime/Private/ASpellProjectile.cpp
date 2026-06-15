#include "ASpellProjectile.h"
#include "AEnemy.h"
#include "AEnemyManager.h"
#include "AVoxelWorldActor.h"
#include "MaterialType.h"

ASpellProjectile::ASpellProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ASpellProjectile::Init(FGridPos InOrigin, FIntVector InDir,
                             AEnemyManager* InEnemyMgr, AVoxelWorldActor* InVoxelWorld)
{
    CurrentPos = InOrigin;
    TileDir    = InDir;
    EnemyMgr   = InEnemyMgr;
    VoxelWorld = InVoxelWorld;
    SetActorLocation(FVector((float)CurrentPos.X, (float)CurrentPos.Y, (float)CurrentPos.Z));
}

void ASpellProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    MoveTimer += DeltaTime;
    while (MoveTimer >= MoveInterval)
    {
        MoveTimer -= MoveInterval;
        AdvanceOneTile();
        if (!IsValid(this)) return;  // destroyed inside AdvanceOneTile
    }
}

void ASpellProjectile::AdvanceOneTile()
{
    if (TilesTravelled >= MaxRange)
    {
        Destroy();
        return;
    }

    FGridPos NextPos(
        CurrentPos.X + TileDir.X,
        CurrentPos.Y + TileDir.Y,
        CurrentPos.Z + TileDir.Z
    );

    // 先檢查是否命中敵人（優先於 tile 碰撞）
    if (AEnemy* Hit = FindEnemyAt(NextPos))
    {
        if (OnHitEnemy) OnHitEnemy(Hit, NextPos);
        Destroy();
        return;
    }

    // 檢查 tile 是否實心（非 Air）
    if (VoxelWorld)
    {
        FTileWorld3D* TW = VoxelWorld->GetTileWorld();
        if (TW)
        {
            EMaterialType Mat = TW->GetTile(NextPos.X, NextPos.Y, NextPos.Z);
            if (Mat != EMaterialType::Air)
            {
                Destroy();
                return;
            }
        }
    }

    CurrentPos = NextPos;
    ++TilesTravelled;
    SetActorLocation(FVector((float)CurrentPos.X, (float)CurrentPos.Y, (float)CurrentPos.Z));
}

AEnemy* ASpellProjectile::FindEnemyAt(const FGridPos& Pos) const
{
    if (!EnemyMgr) return nullptr;
    for (AEnemy* E : EnemyMgr->GetEnemies())
    {
        if (E && E->IsAlive() && E->GetPosition() == Pos)
            return E;
    }
    return nullptr;
}
