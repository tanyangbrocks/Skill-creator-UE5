#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NPCIdentity.h"
#include "UNPCIdentityGeneratorSubsystem.generated.h"

// Per-world subsystem. Loads an NPC's identity from disk if one already exists;
// otherwise asks the LLM (via UNPCBrainSubsystem) to generate one and saves it.
// Once saved, an identity is locked — this subsystem never regenerates or
// overwrites an identity that already has a save file on disk.
// Access via: UNPCIdentityGeneratorSubsystem::Get(World)
UCLASS()
class NPCBRAIN_API UNPCIdentityGeneratorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static UNPCIdentityGeneratorSubsystem* Get(const UObject* WorldCtx);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// OnReady fires exactly once, on the game thread. If NPCId has no save file
	// and the brain subsystem (llama-server) isn't available, OnReady never fires
	// and an error is logged.
	void LoadOrGenerate(FName NPCId, FOnIdentityReady OnReady);

private:
	void RequestGeneration(FName NPCId, FOnIdentityReady OnReady);
	void ParseAndSave(FName NPCId, const FString& LlmResponseJson, FOnIdentityReady OnReady);
};
