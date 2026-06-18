#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NPCBrainSettings.generated.h"

UCLASS(config = NPCBrain, defaultconfig, meta = (DisplayName = "NPC Brain"))
class NPCBRAIN_API UNPCBrainSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UNPCBrainSettings();

	// Full path to llama-server.exe
	UPROPERTY(config, EditAnywhere, Category = "Server")
	FString LlamaServerExePath;

	// Full path to the .gguf model file
	UPROPERTY(config, EditAnywhere, Category = "Server")
	FString ModelPath;

	// Port llama-server listens on (default 8080)
	UPROPERTY(config, EditAnywhere, Category = "Server")
	int32 Port = 8080;

	// Token context window size
	UPROPERTY(config, EditAnywhere, Category = "Model")
	int32 ContextSize = 4096;

	// CPU threads for inference
	UPROPERTY(config, EditAnywhere, Category = "Model")
	int32 NumThreads = 4;

	// Seconds to wait for server ready before giving up
	UPROPERTY(config, EditAnywhere, Category = "Server")
	float ReadyTimeoutSeconds = 60.0f;

	// Launch llama-server automatically when game starts
	UPROPERTY(config, EditAnywhere, Category = "Server")
	bool bAutoLaunchServer = true;

	// System prompt injection point for NPC identity generation (M-NPC-1).
	// Lightweight default referencing the 蒼究 worldview — see
	// docs/plan-worldlore-integration.md for the full lore; deeper integration
	// is W-series scope, out of scope here.
	UPROPERTY(config, EditAnywhere, Category = "Worldview", meta = (MultiLine = true))
	FString WorldviewSystemPrompt;

	static const UNPCBrainSettings* Get()
	{
		return GetDefault<UNPCBrainSettings>();
	}
};
