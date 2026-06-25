#pragma once
#include "CoreMinimal.h"
#include "NPCActionTypes.h"

// M-NPC-4: parses the raw LLM string into FNPCBrainResponse.
// Handles chain-of-thought <think>…</think> stripping, then extracts
// <response>…</response> (or bare JSON) and deserialises the four fields.
struct NPCBRAIN_API FNPCResponseParser
{
    static FNPCBrainResponse Parse(const FString& RawResponse);

private:
    static FString    StripThinkBlock(const FString& Raw);
    static FString    ExtractPayload(const FString& Raw);
    static ENPCAction ActionFromString(const FString& S);
};
