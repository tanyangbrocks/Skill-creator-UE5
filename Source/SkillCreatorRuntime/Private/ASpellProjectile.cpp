#include "ASpellProjectile.h"
#include "AEnemy.h"
#include "AEnemyManager.h"
#include "AVoxelWorldActor.h"
#include "ASkillCreatorCharacter.h"

ASpellProjectile::ASpellProjectile()
{
    // ABaseProjectile constructor 已設 PrimaryActorTick.bCanEverTick = true
}

void ASpellProjectile::Init(FGridPos InOrigin, FVector InDir,
                             AEnemyManager* InEnemyMgr, AVoxelWorldActor* InVoxelWorld)
{
    EnemyMgr = InEnemyMgr;
    InitBase(InOrigin, InDir, InVoxelWorld);
}

void ASpellProjectile::OnTileEntered(FGridPos NewTile)
{
    // 敵人投射物：命中玩家 tile → 走 B-3 物理傷害管線（含 S-4 彈反）
    if (PlayerTarget.IsValid() && PlayerTarget->IsAlive()
        && PlayerTarget->GetPosition() == NewTile)
    {
        // C-2: 傳 OriginEnemy（投射物發射者）而非 this，S-4 彈反才能凍結正確目標
        PlayerTarget->TakePhysicalDamage(BaseDamage,
            bUseAttackerStats ? &AttackerStats : nullptr,
            OriginEnemy.Get());
        Destroy();
        return;
    }

    // 玩家技能：命中敵人 → OnHitEnemy 回調
    if (AEnemy* Hit = FindEnemyAt(NewTile))
    {
        if (OnHitEnemy) OnHitEnemy(Hit, NewTile);
        Destroy();
        return;
    }
}

AEnemy* ASpellProjectile::FindEnemyAt(const FGridPos& Pos) const
{
    if (!EnemyMgr) return nullptr;
    for (AEnemy* E : EnemyMgr->GetEnemies())
    {
        if (!E || !E->IsAlive()) continue;
        if (E->GetPosition() == Pos) return E;
        // Heavy 佔 2×2 footprint（X/Z 各延伸 1 格），命中任一佔用格即觸發
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
