#include "ABaseProjectile.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "MaterialType.h"

ABaseProjectile::ABaseProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ABaseProjectile::InitBase(FGridPos InOrigin, FVector InDir, AVoxelWorldActor* InVoxelWorld)
{
    CurrentPos = InOrigin;
    TileDir    = InDir.GetSafeNormal();
    FloatPos   = FVector((float)InOrigin.X, (float)InOrigin.Y, (float)InOrigin.Z);
    VoxelWorld = InVoxelWorld;
    SetActorLocation(FloatPos);
}

void ABaseProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    MoveTimer += DeltaTime;
    while (MoveTimer >= MoveInterval)
    {
        MoveTimer -= MoveInterval;
        AdvanceOneTile();
        if (!IsValid(this)) return;
    }
}

void ABaseProjectile::AdvanceOneTile()
{
    if (TilesTravelled >= MaxRange)
    {
        Destroy();
        return;
    }

    FloatPos += TileDir;

    FGridPos NextPos(
        FMath::RoundToInt(FloatPos.X),
        FMath::RoundToInt(FloatPos.Y),
        FMath::RoundToInt(FloatPos.Z)
    );

    // 對角線移動時浮點累積未跨整格，繼續等
    if (NextPos == CurrentPos)
    {
        ++TilesTravelled;
        return;
    }

    // ① 子類先做生物/玩家命中偵測；命中時 Destroy() 後跳出
    OnTileEntered(NextPos);
    if (!IsValid(this)) return;

    // ② 再檢查實心 tile 碰撞
    if (VoxelWorld)
    {
        if (FTileWorld3D* TW = VoxelWorld->GetTileWorld())
        {
            if (TW->GetTile(NextPos.X, NextPos.Y, NextPos.Z) != EMaterialType::Air)
            {
                OnHitSolidTile(NextPos);
                return;
            }
        }
    }

    CurrentPos = NextPos;
    ++TilesTravelled;
    SetActorLocation(FloatPos);
}
