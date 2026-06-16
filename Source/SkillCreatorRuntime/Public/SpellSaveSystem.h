#pragma once
#include "CoreMinimal.h"
#include "SpellGroup.h"

// FSpellGroup ↔ JSON 字串序列化（W-6 SpellSaveSystem）。
// 對應 Godot AbilitySystem/SaveSystem.cs：SaveGroupToString / LoadGroupFromString。
// 注意：FSpellArray.ContainerEffect（TSharedPtr，非 UPROPERTY）暫不序列化（deferred 至 M-9）。
struct SKILLCREATORRUNTIME_API FSpellSaveSystem
{
    // FSpellGroup 全組序列化為 JSON 字串，供存入 FCharacterSaveData.SpellGroupJson
    static FString SaveGroupToString(const FSpellGroup& Group);

    // JSON 字串還原 FSpellGroup；失敗時回傳 false，OutGroup 保持不變
    static bool LoadGroupFromString(const FString& Json, FSpellGroup& OutGroup);
};
