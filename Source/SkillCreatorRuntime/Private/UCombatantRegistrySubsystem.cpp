#include "UCombatantRegistrySubsystem.h"
#include "ICombatant.h"

void UCombatantRegistrySubsystem::Register(ICombatant* C)
{
    if (!C) return;
    Combatants.AddUnique(C);
    if (AActor* A = C->AsActor())
        CombatantActorRefs.Add(A);
}

void UCombatantRegistrySubsystem::Unregister(ICombatant* C)
{
    if (!C) return;
    Combatants.Remove(C);
    if (AActor* A = C->AsActor())
        CombatantActorRefs.Remove(A);
}

ICombatant* UCombatantRegistrySubsystem::FindHostileAt(FGridPos Pos) const
{
    for (ICombatant* C : Combatants)
    {
        if (!C || !C->IsAlive() || !C->IsHostile()) continue;
        if (C->OccupiesTile(Pos)) return C;
    }
    return nullptr;
}

TArray<ICombatant*> UCombatantRegistrySubsystem::GetAllHostile() const
{
    TArray<ICombatant*> Out;
    for (ICombatant* C : Combatants)
    {
        if (C && C->IsAlive() && C->IsHostile())
            Out.Add(C);
    }
    return Out;
}
