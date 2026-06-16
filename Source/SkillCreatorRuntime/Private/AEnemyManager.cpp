#include "AEnemyManager.h"
#include "UCombatStateSubsystem.h"
#include "UDroppedItemManager.h"
#include "Kismet/GameplayStatics.h"

AEnemyManager::AEnemyManager()
{
    PrimaryActorTick.bCanEverTick = true;
}

AEnemy* AEnemyManager::Spawn(FGridPos Pos, EEnemyType Type, float MaxHp, ESpawnCategory Cat)
{
    FVector WorldLoc(Pos.X, Pos.Y, Pos.Z);
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
                    DropMgr->SpawnDrop(EItemId::FragmentStone, 1, E->GridPosition);

                bool bDynamic = (E->Category == ESpawnCategory::Common || E->Category == ESpawnCategory::Area);
                if (bDynamic)
                {
                    DynamicActiveCount--;
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
