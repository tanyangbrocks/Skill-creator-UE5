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
	// 2026-06-23（docs/plan-base-npc-system.md §三）：新增 SubtypeId 參數——只在「真的要
	// 生成新身分」時才會用到（已有存檔時直接讀回，SubtypeId 被忽略，沿用存檔當時的值）。
	void LoadOrGenerate(FName NPCId, FName SubtypeId, FOnIdentityReady OnReady);

private:
	void RequestGeneration(FName NPCId, FName SubtypeId, FOnIdentityReady OnReady);
	void ParseAndSave(FName NPCId, FName SubtypeId, FName RaceId, const FString& LlmResponseJson, FOnIdentityReady OnReady);

	// 從 FRaceRegistry 全部體系裡隨機抽一個種族（不再讓 LLM 自由發明種族名稱）
	static FName PickRandomRaceId();
};
