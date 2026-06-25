#pragma once
#include "CoreMinimal.h"
#include "NPCBrainTypes.h"
#include "NPCIdentity.h"
#include "WorldSnapshot.h"

class UNPCMemoryComponent;

// M-NPC-4: assembles the full LLM message array for one dialogue round.
// system  = worldview config + identity role-play instructions + response format
// user    = long-term summary + permanent memories + recent events + perception + player input
struct NPCBRAIN_API FPromptBuilder
{
    static TArray<FNPCMessage> Build(
        const FNPCIdentity&        Identity,
        const UNPCMemoryComponent* Memory,
        const FNPCWorldSnapshot&   Snapshot,
        const FString&             PlayerInput);

private:
    static FString BuildSystemBlock(const FNPCIdentity& Identity);
    static FString BuildUserBlock(const UNPCMemoryComponent* Memory,
                                  const FNPCWorldSnapshot&   Snapshot,
                                  const FString&             PlayerInput);
};
