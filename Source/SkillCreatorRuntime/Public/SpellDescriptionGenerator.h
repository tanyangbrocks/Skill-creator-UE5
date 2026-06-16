#pragma once
#include "CoreMinimal.h"
struct FSpellArray;
struct FSpellSlot;
struct FEngraveData;

// 技能描述文字產生器（對應 Godot SpellDescriptionGenerator.cs / UI 重構 Stage 5）
// 純靜態工具，不依賴 UObject 系統，可在任何 C++ 層呼叫
struct SKILLCREATORRUNTIME_API FSpellDescriptionGenerator
{
    // 結構化描述（機器可讀格式，適合 Tooltip 詳情面板）
    static FString GenerateStructured(const FSpellArray& Spell);

    // 自然語言描述（玩家可讀散文，適合技能介紹/Lore 文字）
    static FString GenerateProse(const FSpellArray& Spell);

private:
    static FString DescribeSlot(const FSpellSlot& Slot, int32 Index);
    static FString DescribeEngraving(const FEngraveData& Eng, bool bShort = false);
    static FString ColorName(uint8 Color);
    static FString TriggerName(uint8 Trigger);
    static FString ContainerName(uint8 Container);
    static FString ElementName(uint8 Elem);
    static FString TotemTypeName(uint8 Type);
    static FString ActivationName(uint8 Act);
};
