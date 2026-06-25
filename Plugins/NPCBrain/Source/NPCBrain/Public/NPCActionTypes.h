#pragma once
#include "CoreMinimal.h"
#include "NPCActionTypes.generated.h"

// M-NPC-4: actions the LLM can request; M-NPC-6 dispatches each to game behavior.
UENUM(BlueprintType)
enum class ENPCAction : uint8
{
    Idle,
    Attack,
    Flee,
    Follow,
    Trade,
    CastSpell,
    PlaceTile,
    BreakTile,
    GiveItem,
};

// Parsed output of one LLM inference call (C++-internal; ANPCCharacter exposes
// the individual string fields to Blueprint rather than this whole struct).
struct NPCBRAIN_API FNPCBrainResponse
{
    FString    Dialogue;
    ENPCAction Action     = ENPCAction::Idle;
    FString    Emotion;
    FString    MemoryNote;
    bool       bValid     = false;
};
