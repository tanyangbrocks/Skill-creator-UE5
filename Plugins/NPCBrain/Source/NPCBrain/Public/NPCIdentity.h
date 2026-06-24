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

	// 2026-06-23（docs/plan-base-npc-system.md §三）：規格要求「名字、性格、外貌、種族、
	// 背景勢力、身份」六項，原本缺「外貌」；種族改成存 FRaceRegistry 的 Id（不再讓 LLM
	// 自由發明種族名稱），SubtypeId 對應 FNPCKindRegistry（e.g. "WanderingBard"）。
	// 兩者在生成「之前」由呼叫端/UNPCIdentityGeneratorSubsystem 設好，LLM JSON 回應不包含
	// 這兩個 key，反序列化時不會被覆寫。
	UPROPERTY() FString Appearance;
	UPROPERTY() FName   RaceId;
	UPROPERTY() FName   SubtypeId;

	bool IsValid() const { return !NPCId.IsNone() && !Species.IsEmpty(); }

	// {ProjectSavedDir}/NPCBrain/Identities/{NPCId}.json
	static FString FilePath(FName InNPCId);

	bool Save() const;
	static bool Load(FName InNPCId, FNPCIdentity& Out);
};

// Fired once, on the game thread, when an identity is ready (loaded from disk
// or freshly generated and saved).
DECLARE_DELEGATE_OneParam(FOnIdentityReady, const FNPCIdentity& /*Identity*/);
