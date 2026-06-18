#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Containers/Ticker.h"
#include "NPCBrainTypes.h"
#include "LlamaServerProcess.h"
#include "LlamaInferenceClient.h"
#include "UNPCBrainSubsystem.generated.h"

// Per-world subsystem. Owns the llama-server process and inference client.
// Access via: UNPCBrainSubsystem::Get(World)
UCLASS()
class NPCBRAIN_API UNPCBrainSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	// Convenience getter
	static UNPCBrainSubsystem* Get(const UObject* WorldCtx);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// True when the server has responded to the /health endpoint
	bool IsReady() const { return bServerReady; }

	// Send messages to LLM. Queues the request if server is not yet ready.
	// Callbacks fire on the game thread.
	void SendMessages(
		const TArray<FNPCMessage>& Messages,
		FOnInferenceComplete       OnComplete,
		FOnInferenceError          OnError,
		float Temperature  = 0.8f,
		int32 MaxTokens    = 512);

	// Fired once when the server becomes ready
	DECLARE_MULTICAST_DELEGATE(FOnServerReady);
	FOnServerReady OnServerReady;

private:
	FLlamaServerProcess   ServerProcess;
	FLlamaInferenceClient InferenceClient;

	bool           bServerReady     = false;
	double         ReadyPollEndTime = 0.0;
	FTSTicker::FDelegateHandle TickerHandle;

	bool PollServerTick(float DeltaTime);
};
