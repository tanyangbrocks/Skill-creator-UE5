#include "FPromptBuilder.h"
#include "NPCBrainSettings.h"
#include "NPCMemoryTypes.h"
#include "UNPCMemoryComponent.h"

TArray<FNPCMessage> FPromptBuilder::Build(
    const FNPCIdentity&        Identity,
    const UNPCMemoryComponent* Memory,
    const FNPCWorldSnapshot&   Snapshot,
    const FString&             PlayerInput)
{
    TArray<FNPCMessage> Messages;

    FNPCMessage Sys;
    Sys.Role    = ENPCMessageRole::System;
    Sys.Content = BuildSystemBlock(Identity);
    Messages.Add(Sys);

    FNPCMessage User;
    User.Role    = ENPCMessageRole::User;
    User.Content = BuildUserBlock(Memory, Snapshot, PlayerInput);
    Messages.Add(User);

    return Messages;
}

FString FPromptBuilder::BuildSystemBlock(const FNPCIdentity& Identity)
{
    const UNPCBrainSettings* Cfg = GetDefault<UNPCBrainSettings>();

    FString Block;

    if (Cfg && !Cfg->WorldviewSystemPrompt.IsEmpty())
        Block += Cfg->WorldviewSystemPrompt + TEXT("\n\n");

    Block += FString::Printf(
        TEXT("You are %s, a %s %s"),
        *Identity.Name,
        *Identity.Species,
        *Identity.Role);

    if (!Identity.Faction.IsEmpty())
        Block += FString::Printf(TEXT(" from the faction \"%s\""), *Identity.Faction);

    Block += TEXT(".\n");

    if (!Identity.Backstory.IsEmpty())
        Block += TEXT("Background: ") + Identity.Backstory + TEXT("\n");

    if (Identity.Traits.Num() > 0)
        Block += TEXT("Personality traits: ") + FString::Join(Identity.Traits, TEXT(", ")) + TEXT("\n");

    if (!Identity.SpeechStyle.IsEmpty())
        Block += TEXT("Speech style: ") + Identity.SpeechStyle + TEXT("\n");

    if (!Identity.Appearance.IsEmpty())
        Block += TEXT("Appearance: ") + Identity.Appearance + TEXT("\n");

    Block += TEXT(
        "\nAlways respond with a JSON object wrapped in <response>...</response> tags:\n"
        "<response>\n"
        "{\n"
        "  \"dialogue\": \"<what you say aloud, in character>\",\n"
        "  \"action\": \"Idle|Attack|Flee|Follow|Trade|CastSpell|PlaceTile|BreakTile|GiveItem\",\n"
        "  \"emotion\": \"<current emotion>\",\n"
        "  \"memory_note\": \"<one sentence to remember, or empty string>\"\n"
        "}\n"
        "</response>\n"
        "You may use <think>...</think> before the response to reason privately.\n"
        "Never deviate from the JSON format inside <response>.\n");

    return Block;
}

FString FPromptBuilder::BuildUserBlock(
    const UNPCMemoryComponent* Memory,
    const FNPCWorldSnapshot&   Snapshot,
    const FString&             PlayerInput)
{
    FString Block;

    if (Memory)
    {
        const FString& LtSummary = Memory->GetLongTermSummary();
        if (!LtSummary.IsEmpty())
            Block += TEXT("[Long-term memory]\n") + LtSummary + TEXT("\n\n");

        const TArray<FNPCMemoryEvent>& Perm = Memory->GetPermanentMemory();
        if (Perm.Num() > 0)
        {
            Block += TEXT("[Permanent memory]\n");
            for (const FNPCMemoryEvent& E : Perm)
                Block += TEXT("- ") + E.Content + TEXT("\n");
            Block += TEXT("\n");
        }

        const TArray<FNPCMemoryEvent>& St = Memory->GetShortTermMemory();
        if (St.Num() > 0)
        {
            Block += TEXT("[Recent events]\n");
            for (const FNPCMemoryEvent& E : St)
                Block += TEXT("- ") + E.Content + TEXT("\n");
            Block += TEXT("\n");
        }
    }

    if (Snapshot.NearbyCreatureIds.Num() > 0)
        Block += FString::Printf(
            TEXT("[Perception] %d creature(s) nearby.\n"), Snapshot.NearbyCreatureIds.Num());

    if (Snapshot.HazardMaterials.Num() > 0)
        Block += TEXT("[Perception] Hazardous materials detected in the vicinity.\n");

    Block += TEXT("\n");

    if (!PlayerInput.IsEmpty())
        Block += TEXT("Player says: \"") + PlayerInput + TEXT("\"");
    else
        Block += TEXT("(No player input — assess the situation and choose your action.)");

    return Block;
}
