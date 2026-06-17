#pragma once
#include "CoreMinimal.h"
#include "NPCBrainTypes.generated.h"

UENUM(BlueprintType)
enum class ENPCMessageRole : uint8
{
	System    UMETA(DisplayName = "System"),
	User      UMETA(DisplayName = "User"),
	Assistant UMETA(DisplayName = "Assistant"),
};

USTRUCT(BlueprintType)
struct NPCBRAIN_API FNPCMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ENPCMessageRole Role = ENPCMessageRole::User;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Content;
};

// Called on game thread when inference succeeds
DECLARE_DELEGATE_OneParam(FOnInferenceComplete, const FString& /*Response*/);

// Called on game thread when inference fails
DECLARE_DELEGATE_TwoParams(FOnInferenceError, int32 /*HttpStatusCode*/, const FString& /*ErrorMsg*/);
