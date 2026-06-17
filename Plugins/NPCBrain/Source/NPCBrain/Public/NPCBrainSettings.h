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

	static const UNPCBrainSettings* Get()
	{
		return GetDefault<UNPCBrainSettings>();
	}
};
