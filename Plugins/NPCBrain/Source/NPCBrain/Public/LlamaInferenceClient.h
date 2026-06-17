#pragma once
#include "CoreMinimal.h"
#include "NPCBrainTypes.h"
// HTTP client for llama-server's OpenAI-compatible /v1/chat/completions endpoint.
class NPCBRAIN_API FLlamaInferenceClient
{
public:
	FLlamaInferenceClient() = default;

	void Configure(const FString& InBaseUrl);

	// Post messages to LLM. Callbacks fire on the game thread.
	void SendMessages(
		const TArray<FNPCMessage>& Messages,
		FOnInferenceComplete       OnComplete,
		FOnInferenceError          OnError,
		float Temperature  = 0.8f,
		int32 MaxTokens    = 512);

	bool IsConfigured() const { return !BaseUrl.IsEmpty(); }

private:
	FString BaseUrl;

	static FString RoleToString(ENPCMessageRole Role);
};
