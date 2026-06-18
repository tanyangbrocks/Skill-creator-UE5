#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPCMemoryTypes.h"
#include "UNPCMemoryComponent.generated.h"

// M-NPC-2: short-term/long-term memory for one NPC.
// Short-term buffer is capped; hitting the cap triggers an LLM compression pass
// that folds it into a running long-term summary string. Permanent events
// (bPermanent=true) bypass the buffer entirely and are never evicted.
//
// Plan called for TCircularBuffer<FNPCMemoryEvent>; a plain TArray with
// FIFO eviction is used instead — at a 20-entry cap the cost difference is
// immaterial, and TArray avoids TCircularBuffer's power-of-2 capacity and
// index-wraparound bookkeeping for no real benefit here.
UCLASS(ClassGroup = SkillCreator, meta = (BlueprintSpawnableComponent))
class NPCBRAIN_API UNPCMemoryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNPCMemoryComponent();

	static constexpr int32 ShortTermMemoryCap = 20;

	// Routes to PermanentMemory (never evicted) or ShortTermMemory (FIFO, capped;
	// hitting the cap triggers async LLM compression into LongTermSummary).
	void AddMemoryEvent(ENPCMemoryCategory Category, const FString& Content, bool bPermanent = false);

	const TArray<FNPCMemoryEvent>& GetShortTermMemory() const { return ShortTermMemory; }
	const TArray<FNPCMemoryEvent>& GetPermanentMemory()  const { return PermanentMemory; }
	const FString&                 GetLongTermSummary()  const { return LongTermSummary; }
	bool                           IsCompressionInFlight() const { return bCompressionInFlight; }

private:
	UPROPERTY() TArray<FNPCMemoryEvent> ShortTermMemory;
	UPROPERTY() TArray<FNPCMemoryEvent> PermanentMemory;
	UPROPERTY() FString                 LongTermSummary;

	TArray<FNPCMemoryEvent> PendingCompressionBatch;
	bool bCompressionInFlight = false;

	void RequestCompression();
	void OnCompressionComplete(const FString& Summary);
	void OnCompressionError(int32 HttpStatusCode, const FString& ErrorMsg);
};
