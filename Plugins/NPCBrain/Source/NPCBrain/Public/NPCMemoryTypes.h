#pragma once
#include "CoreMinimal.h"
#include "NPCMemoryTypes.generated.h"

UENUM(BlueprintType)
enum class ENPCMemoryCategory : uint8
{
	Observe UMETA(DisplayName = "觀察"),
	Speak   UMETA(DisplayName = "說話"),
	Receive UMETA(DisplayName = "聽到"),
	World   UMETA(DisplayName = "世界事件"),
};

USTRUCT(BlueprintType)
struct NPCBRAIN_API FNPCMemoryEvent
{
	GENERATED_BODY()

	UPROPERTY() FDateTime Timestamp;
	UPROPERTY() ENPCMemoryCategory Category = ENPCMemoryCategory::Observe;
	UPROPERTY() FString Content;

	// Permanent events are never evicted by short-term buffer compression
	// (M-NPC-2 "重要事件" requirement).
	UPROPERTY() bool bPermanent = false;
};
