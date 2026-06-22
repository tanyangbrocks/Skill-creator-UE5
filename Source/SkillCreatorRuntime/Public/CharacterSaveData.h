#pragma once
#include "CoreMinimal.h"
#include "SpellArray.h"
#include "ItemStack.h"
#include "CharacterSaveData.generated.h"

// JSON save data for the player character.
// Serialized with FJsonObjectConverter — only UPROPERTY fields survive.
// FBlockNode tree (block AST) is deferred to M-9 (Slate editor).
// FSpellArray::ContainerEffect (TSharedPtr, non-UPROPERTY) is also skipped.
//
// 角色獨立於世界存在（對應 Godot 全域 _chars 清單，可任意搭配世界進場）：
// 存檔位置 {ProjectSavedDir}/Characters/{Id}.json，不綁定任何特定世界資料夾。
USTRUCT()
struct SKILLCREATORRUNTIME_API FCharacterSaveData
{
    GENERATED_BODY()

    UPROPERTY() FString    Id;
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

    // B-2：角色年齡（玩家自填，非計算值）
    UPROPERTY() int32 CharacterAge  = 0;   // 角色設定年齡
    UPROPERTY() int32 AppearanceAge = 0;   // 外貌年齡（可與角色年齡不同）

    // W-10：角色創建系統（對應 docs/plan-w10-character-creation.md §3.2）
    UPROPERTY() FName RaceId;
    UPROPERTY() int32 BasePoint_Physique  = 0; // 體魄
    UPROPERTY() int32 BasePoint_Strength  = 0; // 肌力
    UPROPERTY() int32 BasePoint_Endurance = 0; // 耐力
    UPROPERTY() int32 BasePoint_Agility   = 0; // 敏捷
    UPROPERTY() int32 BasePoint_Intellect = 0; // 智慧
    UPROPERTY() int32 BasePoint_Charisma  = 0; // 魅力
    UPROPERTY() int32 BasePoint_Luck      = 0; // 幸運

    // {ProjectSavedDir}/Characters/{Id}.json
    static FString FilePath(const FString& InId);

    bool        Save() const;
    static bool Load(const FString& InId, FCharacterSaveData& Out);
};
