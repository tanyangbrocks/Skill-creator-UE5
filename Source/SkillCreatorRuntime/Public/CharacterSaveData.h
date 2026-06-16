#pragma once
#include "CoreMinimal.h"
#include "SpellArray.h"
#include "ItemStack.h"
#include "CharacterSaveData.generated.h"

// JSON save data for the player character.
// Serialized with FJsonObjectConverter — only UPROPERTY fields survive.
// FBlockNode tree (block AST) is deferred to M-9 (Slate editor).
// FSpellArray::ContainerEffect (TSharedPtr, non-UPROPERTY) is also skipped.
USTRUCT()
struct SKILLCREATORRUNTIME_API FCharacterSaveData
{
    GENERATED_BODY()

    UPROPERTY() FString    CharacterName;
    UPROPERTY() FIntVector TilePosition  = FIntVector(0, 0, 0);
    UPROPERTY() float      CurrentHp     = 100.f;
    UPROPERTY() float      CurrentMp     = 100.f;
    UPROPERTY() float      Stamina       = 100.f;
    UPROPERTY() float      MentalEnergy  = 100.f;
    UPROPERTY() float      Mood          = 70.f;

    // S-1: 補完欄位
    UPROPERTY() int32               Level         = 1;
    UPROPERTY() float               Xp            = 0.f;
    UPROPERTY() TArray<FItemStack>  InventorySlots;   // 30 格背包（對應 UInventoryComponent.Slots）
    UPROPERTY() int32               ActiveHotbar  = 0; // 目前選中熱鍵欄格
    UPROPERTY() TMap<FName, float>  ManaCurrents;      // ManaTypeKey → 目前法力值

    // W-6: FSpellGroup 全組序列化 JSON（ActiveGroupIndex + 5 組 × 10 槽 + 被動）
    // 由 FSpellSaveSystem::SaveGroupToString 產生，LoadGroupFromString 還原
    UPROPERTY() FString    SpellGroupJson;

    // Player's collected spell builds.
    // Each FSpellArray is serialized by its UPROPERTY fields (Name, Slots, etc.).
    UPROPERTY() TArray<FSpellArray> SpellBook;

    // {WorldDir}/character.json
    static FString FilePath(const FString& WorldDir);

    bool        Save(const FString& WorldDir) const;
    static bool Load(const FString& WorldDir, FCharacterSaveData& Out);
};
