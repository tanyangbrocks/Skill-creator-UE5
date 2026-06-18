#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GridPos.h"
#include "WorldSnapshot.h"
#include "UNPCPerceptionComponent.generated.h"

class IWorldInterface;

// M-NPC-3: periodically scans the world around the owning NPC and writes
// notable changes into a sibling UNPCMemoryComponent (if present) as World
// category memory events.
//
// IWorldInterface currently has zero concrete implementations anywhere in the
// codebase (AVoxelWorldActor/AEnemyManager don't implement it yet — a
// pre-existing gap, not new tech debt from this work). Until something calls
// SetWorldInterface() with a real adapter, this component is an inert no-op;
// the diffing/description logic (FNPCPerceptionLogic) is fully unit-tested
// independent of that wiring.
//
// Driven by an explicit TickPerception(DeltaTime, OwnerPos) call from the
// owner, not engine TickComponent — matches this project's manual-accumulator
// convention (see SpellRunner) and keeps this component testable without a
// live world. Named TickPerception rather than Tick to avoid hiding
// UActorComponent::Tick(float), which is declared final.
UCLASS(ClassGroup = SkillCreator, meta = (BlueprintSpawnableComponent))
class NPCBRAIN_API UNPCPerceptionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNPCPerceptionComponent();

	// Sensing range in tiles (Chebyshev distance). Set per-species by the spawner.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
	int32 PerceptionRadiusTiles = 8;

	// How often a perception scan runs. Scans are O(Radius^3) tile lookups, so
	// this is intentionally not every frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
	float PerceptionIntervalSeconds = 2.f;

	// Dependency injection point — see class comment.
	void SetWorldInterface(IWorldInterface* InWorld) { WorldInterface = InWorld; }

	void TickPerception(float DeltaTime, FGridPos OwnerPos);

private:
	IWorldInterface* WorldInterface = nullptr;
	FWorldSnapshot   LastSnapshot;
	float            TimeAccumulator = 0.f;
	bool             bHasSnapshot = false;

	void PerformPerceptionTick(FGridPos OwnerPos);
};
