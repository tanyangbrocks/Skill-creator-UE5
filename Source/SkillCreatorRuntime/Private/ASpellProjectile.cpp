#include "ASpellProjectile.h"
#include "AEnemy.h"
#include "AEnemyManager.h"
#include "AVoxelWorldActor.h"
#include "MaterialType.h"

ASpellProjectile::ASpellProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ASpellProjectile::Init(FGridPos InOrigin, FVector InDir,
                             AEnemyManager* InEnemyMgr, AVoxelWorldActor* InVoxelWorld)
{
    CurrentPos = InOrigin;
    TileDir    = InDir.GetSafeNormal();  // 正規化；零向量時回傳 (0,0,0) → 不移動
    FloatPos   = FVector((float)InOrigin.X, (float)InOrigin.Y, (float)InOrigin.Z);
    EnemyMgr   = InEnemyMgr;
    VoxelWorld = InVoxelWorld;
    SetActorLocation(FloatPos);
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

    // 累積浮點位置：每步沿正規化方向前進一個單位長度
    FloatPos += TileDir;

    // 四捨五入得到當前所在的 tile 格座標（支援對角線投射）
    FGridPos NextPos(
        FMath::RoundToInt(FloatPos.X),
        FMath::RoundToInt(FloatPos.Y),
        FMath::RoundToInt(FloatPos.Z)
    );

    if (NextPos == CurrentPos)
    {
        // 對角線移動時可能未跨格，繼續累積直到整格
        ++TilesTravelled;
        return;
    }

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
    SetActorLocation(FloatPos);  // 使用連續浮點位置讓視覺移動平滑
}

AEnemy* ASpellProjectile::FindEnemyAt(const FGridPos& Pos) const
{
    if (!EnemyMgr) return nullptr;
    for (AEnemy* E : EnemyMgr->GetEnemies())
    {
        if (!E || !E->IsAlive()) continue;
        if (E->GetPosition() == Pos) return E;
        // Heavy 佔 2×2 footprint（X/Z 各延伸 1 格），投射物命中任一佔用格即觸發
        if (E->Type == EEnemyType::Heavy)
        {
            const FGridPos EP = E->GetPosition();
            if (Pos.Y == EP.Y)
                for (int32 DX = 0; DX <= 1; ++DX)
                    for (int32 DZ = 0; DZ <= 1; ++DZ)
                        if (EP.X + DX == Pos.X && EP.Z + DZ == Pos.Z)
                            return E;
        }
    }
    return nullptr;
}
