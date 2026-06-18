#pragma once
#include "CoreMinimal.h"
#include "NPCIdentity.generated.h"

// M-NPC-1: static identity for one NPC instance. Generated once via LLM, then
// persisted to disk and never regenerated — see UNPCIdentityGeneratorSubsystem.
USTRUCT(BlueprintType)
struct NPCBRAIN_API FNPCIdentity
{
	GENERATED_BODY()

	// Stable lookup key / save file name. Caller-assigned, never written by the LLM.
	UPROPERTY()
	FName NPCId;

	UPROPERTY() FString Name;
	UPROPERTY() FString Species;
	UPROPERTY() FString Faction;
	UPROPERTY() FString Role;
	UPROPERTY() TArray<FString> Traits;
	UPROPERTY() FString SpeechStyle;
	UPROPERTY() FString Backstory;

	bool IsValid() const { return !NPCId.IsNone() && !Species.IsEmpty(); }

	// {ProjectSavedDir}/NPCBrain/Identities/{NPCId}.json
	static FString FilePath(FName InNPCId);

	bool Save() const;
	static bool Load(FName InNPCId, FNPCIdentity& Out);
};

// Fired once, on the game thread, when an identity is ready (loaded from disk
// or freshly generated and saved).
DECLARE_DELEGATE_OneParam(FOnIdentityReady, const FNPCIdentity& /*Identity*/);
