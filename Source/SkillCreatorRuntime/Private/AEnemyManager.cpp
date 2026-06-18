#include "AEnemyManager.h"
#include "ASpellProjectile.h"
#include "UCombatStateSubsystem.h"
#include "UDroppedItemManager.h"
#include "AVoxelWorldActor.h"
#include "MaterialRegistry.h"
#include "WorldScale.h"
#include "Kismet/GameplayStatics.h"

AEnemyManager::AEnemyManager()
{
    PrimaryActorTick.bCanEverTick = true;
}

AEnemy* AEnemyManager::Spawn(FGridPos Pos, EEnemyType Type, float MaxHp, ESpawnCategory Cat)
{
    int32 WorldH = WorldScale::DefaultWorldHeight;
    if (AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(GetWorld()))
        WorldH = VW->WorldHeight;
    FVector WorldLoc = WorldScale::TileToWorld(Pos, WorldH);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AEnemy* Enemy = GetWorld()->SpawnActor<AEnemy>(AEnemy::StaticClass(), WorldLoc, FRotator::ZeroRotator, Params);
    if (!Enemy) return nullptr;

    Enemy->Type         = Type;
    Enemy->Category     = Cat;
    Enemy->GridPosition = Pos;
    if (MaxHp > 0.f) Enemy->MaxHp = MaxHp;

    Enemies.Add(Enemy);
    if (Cat == ESpawnCategory::Common || Cat == ESpawnCategory::Area)
        DynamicActiveCount++;

    return Enemy;
}

void AEnemyManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 清理失效的敵人投射物指針
    EnemyProjectiles.RemoveAll(
        [](const TObjectPtr<ASpellProjectile>& P) { return !IsValid(P); });

    for (int32 i = Enemies.Num() - 1; i >= 0; --i)
    {
        AEnemy* E = Enemies[i];
        if (!IsValid(E) || !E->IsAlive())
        {
            if (IsValid(E))
            {
                if (auto* GI = GetWorld()->GetGameInstance())
                if (auto* Sub = GI->GetSubsystem<UCombatStateSubsystem>())
                    Sub->OnEnemyKilled();

                if (auto* DropMgr = GetWorld()->GetSubsystem<UDroppedItemManager>())
                {
                    // 依腳下方塊決定掉落物；無法取得世界時 fallback 到碎石
                    EItemId DropItem = EItemId::FragmentStone;
                    if (AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(GetWorld()))
                        if (FTileWorld3D* TW = VW->GetTileWorld())
                        {
                            EItemId Frag = FMaterialRegistry::GetFragmentItem(
                                static_cast<EMaterialType>(
                                    TW->GetTile(E->GridPosition.X, E->GridPosition.Y, E->GridPosition.Z)));
                            if (Frag != EItemId::None) DropItem = Frag;
                        }
                    DropMgr->SpawnDrop(DropItem, 1, E->GridPosition);
                }

                bool bDynamic = (E->Category == ESpawnCategory::Common || E->Category == ESpawnCategory::Area);
                if (bDynamic)
                {
                    DynamicActiveCount = FMath::Max(0, DynamicActiveCount - 1);
                    E->Destroy();
                    Enemies.RemoveAt(i);
                }
                // Named/Boss: 復活邏輯 → M-5 MobSpawnController
            }
            else
            {
                Enemies.RemoveAt(i);
            }
        }
    }
}

void AEnemyManager::ApplyExplosionDamage(FGridPos Center, int32 Radius, float Damage)
{
    int32 R2 = Radius * Radius;
    for (AEnemy* E : Enemies)
    {
        if (!IsValid(E) || !E->IsAlive()) continue;
        FGridPos D = E->GridPosition - Center;
        if (D.X * D.X + D.Y * D.Y + D.Z * D.Z <= R2)
            E->TakeDamageAmount(Damage);
    }
}
