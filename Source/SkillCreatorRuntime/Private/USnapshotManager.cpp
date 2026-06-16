#include "USnapshotManager.h"
#include "ASkillCreatorCharacter.h"
#include "AEnemy.h"
#include "UElementalAuraComponent.h"
#include "UCharacterStateComponent.h"

void USnapshotManager::TakeSnapshot(ASkillCreatorCharacter* Player, const TArray<AEnemy*>& Enemies)
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

    SnapshotStack.Add(MoveTemp(Snap));
}

bool USnapshotManager::ApplyLatest(ASkillCreatorCharacter* Player, const TArray<AEnemy*>& Enemies)
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

    return true;
}
