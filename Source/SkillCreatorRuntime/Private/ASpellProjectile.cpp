#include "ASpellProjectile.h"
#include "AVoxelWorldActor.h"
#include "ASkillCreatorCharacter.h"
#include "ABeastCharacter.h"
#include "ICombatant.h"
#include "UCombatantRegistrySubsystem.h"

ASpellProjectile::ASpellProjectile()
{
    // ABaseProjectile constructor 已設 PrimaryActorTick.bCanEverTick = true
}

void ASpellProjectile::Init(FGridPos InOrigin, FVector InDir, AVoxelWorldActor* InVoxelWorld)
{
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

    // 玩家技能：透過 UCombatantRegistrySubsystem 找 IsHostile 目標
    if (UWorld* W = GetWorld())
    {
        if (UCombatantRegistrySubsystem* Reg = W->GetSubsystem<UCombatantRegistrySubsystem>())
        {
            if (ICombatant* Hit = Reg->FindHostileAt(NewTile))
            {
                if (OnHitEnemy) OnHitEnemy(Hit, NewTile);
                Destroy();
                return;
            }
        }
    }
}
