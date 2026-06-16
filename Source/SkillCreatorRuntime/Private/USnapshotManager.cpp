#include "USnapshotManager.h"
#include "ASkillCreatorCharacter.h"
#include "AEnemy.h"
#include "UElementalAuraComponent.h"
#include "UCharacterStateComponent.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "MaterialType.h"

void USnapshotManager::TakeSnapshot(ASkillCreatorCharacter* Player, const TArray<AEnemy*>& Enemies,
                                     AVoxelWorldActor* VoxelWorld, int32 TileRadius)
{
    FWorldSnapshot Snap;

    // ── 玩家 ──────────────────────────────────────────────────────────────
    if (Player)
    {
        FEntitySnapshot E;
        E.EntityId  = FEntitySnapshot::PlayerEntityId;
        E.Position  = Player->GetPosition();
        E.Hp        = Player->CurrentHp;
        E.Mp        = Player->CurrentMp;
        E.bWasAlive = Player->IsAliveChar();
        if (Player->AuraComp)
            E.Aura = Player->AuraComp->TakeAuraSnapshot();
        if (Player->StateComp)
        {
            E.bHasCharState         = true;
            E.CharState.Stamina      = Player->StateComp->Stamina;
            E.CharState.MentalEnergy = Player->StateComp->MentalEnergy;
            E.CharState.Mood         = Player->StateComp->Mood;
            E.CharState.BodyTemp     = Player->StateComp->BodyTemperature;
            E.CharState.Thirst       = Player->StateComp->Thirst;
            E.CharState.Hunger       = Player->StateComp->Hunger;
            E.CharState.Oxygen       = Player->StateComp->Oxygen;
        }
        E.bHasCharStats = true;
        E.CharStats     = Player->Stats;
        Snap.Entities.Add(MoveTemp(E));
    }

    // ── 敵人 ──────────────────────────────────────────────────────────────
    for (AEnemy* Enemy : Enemies)
    {
        if (!Enemy) continue;
        FEntitySnapshot E;
        E.EntityId  = Enemy->GetCreatureId();
        E.Position  = Enemy->GetPosition();
        E.Hp        = Enemy->Hp;
        E.Mp        = 0.f;
        E.bWasAlive = Enemy->IsAlive();
        if (Enemy->AuraComp)
            E.Aura = Enemy->AuraComp->TakeAuraSnapshot();
        Snap.Entities.Add(MoveTemp(E));
    }

    // ── S-13：球形 tile 快照 ────────────────────────────────────────────────
    if (VoxelWorld && TileRadius > 0 && Player)
    {
        FTileWorld3D* TW = VoxelWorld->GetTileWorld();
        if (TW)
        {
            const FGridPos Center = Player->GetPosition();
            const int32 R = TileRadius;
            const int32 R2 = R * R;
            for (int32 dx = -R; dx <= R; dx++)
            for (int32 dy = -R; dy <= R; dy++)
            for (int32 dz = -R; dz <= R; dz++)
            {
                if (dx*dx + dy*dy + dz*dz > R2) continue;
                FGridPos P(Center.X+dx, Center.Y+dy, Center.Z+dz);
                Snap.TileSnap.Tiles.Add(P, (uint8)TW->GetTile(P.X, P.Y, P.Z));
            }
        }
    }

    SnapshotStack.Add(MoveTemp(Snap));
}

bool USnapshotManager::ApplyLatest(ASkillCreatorCharacter* Player, const TArray<AEnemy*>& Enemies,
                                    AVoxelWorldActor* VoxelWorld)
{
    if (SnapshotStack.IsEmpty()) return false;

    FWorldSnapshot Snap = MoveTemp(SnapshotStack.Last());
    SnapshotStack.RemoveAt(SnapshotStack.Num() - 1);

    // EntityId → 快照 map（O(1) 還原）
    TMap<int32, FEntitySnapshot> SnapMap;
    SnapMap.Reserve(Snap.Entities.Num());
    for (FEntitySnapshot& E : Snap.Entities)
        SnapMap.Add(E.EntityId, MoveTemp(E));

    // ── 還原玩家 ──────────────────────────────────────────────────────────
    if (Player)
    {
        if (FEntitySnapshot* E = SnapMap.Find(FEntitySnapshot::PlayerEntityId))
        {
            Player->CurrentHp = E->Hp;
            Player->CurrentMp = E->Mp;
            if (Player->AuraComp)
                Player->AuraComp->RestoreAuraSnapshot(E->Aura);
            if (Player->StateComp && E->bHasCharState)
            {
                Player->StateComp->Stamina         = E->CharState.Stamina;
                Player->StateComp->MentalEnergy    = E->CharState.MentalEnergy;
                Player->StateComp->Mood            = E->CharState.Mood;
                Player->StateComp->BodyTemperature = E->CharState.BodyTemp;
                Player->StateComp->Thirst          = E->CharState.Thirst;
                Player->StateComp->Hunger          = E->CharState.Hunger;
                Player->StateComp->Oxygen          = E->CharState.Oxygen;
            }
            if (E->bHasCharStats)
                Player->Stats = E->CharStats;
        }
    }

    // ── 還原敵人 ──────────────────────────────────────────────────────────
    for (AEnemy* Enemy : Enemies)
    {
        if (!Enemy) continue;
        if (FEntitySnapshot* E = SnapMap.Find(Enemy->GetCreatureId()))
        {
            Enemy->Hp           = E->Hp;
            Enemy->GridPosition = E->Position;
            if (Enemy->AuraComp)
                Enemy->AuraComp->RestoreAuraSnapshot(E->Aura);
        }
    }

    // ── S-13：還原 tile 世界 ────────────────────────────────────────────────
    if (VoxelWorld && !Snap.TileSnap.Tiles.IsEmpty())
    {
        FTileWorld3D* TW = VoxelWorld->GetTileWorld();
        if (TW)
        {
            for (const auto& Pair : Snap.TileSnap.Tiles)
                TW->SetTile(Pair.Key.X, Pair.Key.Y, Pair.Key.Z, (EMaterialType)Pair.Value);
        }
    }

    return true;
}
