#include "UNPCPerceptionComponent.h"
#include "UNPCMemoryComponent.h"
#include "NPCMemoryTypes.h"
#include "IWorldInterface.h"
#include "ICreature.h"

UNPCPerceptionComponent::UNPCPerceptionComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // manually driven, see Tick()
}

void UNPCPerceptionComponent::TickPerception(float DeltaTime, FGridPos OwnerPos)
{
	if (!WorldInterface) return;

	TimeAccumulator += DeltaTime;
	if (TimeAccumulator < PerceptionIntervalSeconds) return;
	TimeAccumulator = 0.f;

	PerformPerceptionTick(OwnerPos);
}

void UNPCPerceptionComponent::PerformPerceptionTick(FGridPos OwnerPos)
{
	FWorldSnapshot Current;

	for (ICreature* C : WorldInterface->GetEntitiesNear(OwnerPos, (float)PerceptionRadiusTiles))
	{
		if (C && C->IsAlive())
			Current.NearbyCreatureIds.Add(C->GetCreatureId());
	}

	// O(Radius^3) tile scan — fine for a handful of NPCs at modest radius; a
	// candidate for optimization (e.g. chunk-level hazard tracking) if NPC
	// count grows large. Chebyshev distance for the radius test, consistent
	// with the AI attack-range convention (see FGridPos::ChebyshevDistance).
	for (int32 dx = -PerceptionRadiusTiles; dx <= PerceptionRadiusTiles; ++dx)
	for (int32 dy = -PerceptionRadiusTiles; dy <= PerceptionRadiusTiles; ++dy)
	for (int32 dz = -PerceptionRadiusTiles; dz <= PerceptionRadiusTiles; ++dz)
	{
		const FGridPos Probe(OwnerPos.X + dx, OwnerPos.Y + dy, OwnerPos.Z + dz);
		if (OwnerPos.ChebyshevDistance(Probe) > PerceptionRadiusTiles) continue;

		const EMaterialType Mat = WorldInterface->GetMaterialAt(Probe);
		if (FNPCPerceptionLogic::IsHazardMaterial(Mat))
			Current.HazardMaterials.AddUnique(Mat);
	}

	if (bHasSnapshot)
	{
		const TArray<FString> Changes = FNPCPerceptionLogic::DescribeChanges(LastSnapshot, Current);
		if (Changes.Num() > 0)
		{
			UNPCMemoryComponent* Memory = GetOwner()
				? GetOwner()->FindComponentByClass<UNPCMemoryComponent>()
				: nullptr;
			if (Memory)
			{
				for (const FString& Desc : Changes)
					Memory->AddMemoryEvent(ENPCMemoryCategory::World, Desc);
			}
		}
	}

	LastSnapshot  = MoveTemp(Current);
	bHasSnapshot  = true;
}
