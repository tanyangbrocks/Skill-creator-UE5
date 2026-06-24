#pragma once
#include "CoreMinimal.h"
#include "MaterialType.h"
#include "WorldSnapshot.generated.h"

// M-NPC-3: a point-in-time snapshot of what an NPC perceives nearby.
// Deliberately holds only primitive data (no UWorld/IWorldInterface pointers)
// so the diffing logic below is unit-testable without a live world.
USTRUCT(BlueprintType)
struct NPCBRAIN_API FNPCWorldSnapshot
{
	GENERATED_BODY()

	UPROPERTY() TSet<int32> NearbyCreatureIds;
	UPROPERTY() TArray<EMaterialType> HazardMaterials; // deduplicated, presence-only
};

// Pure logic for turning two FNPCWorldSnapshot states into natural-language
// perception sentences. No UWorld/IWorldInterface dependency — gathering a
// live snapshot is the job of UNPCPerceptionComponent.
class NPCBRAIN_API FNPCPerceptionLogic
{
public:
	static bool IsHazardMaterial(EMaterialType Mat);
	static FString MaterialDisplayName(EMaterialType Mat);

	// Returns one sentence per kind of change; empty array if nothing changed.
	static TArray<FString> DescribeChanges(const FNPCWorldSnapshot& Prev, const FNPCWorldSnapshot& Current);
};
