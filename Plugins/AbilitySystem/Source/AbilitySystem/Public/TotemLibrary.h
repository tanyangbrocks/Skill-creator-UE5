#pragma once
#include "CoreMinimal.h"
#include "SpellArray.h"

// 技能因子 + 刻印完整資料庫（對應 Godot UI/TotemLibrary.cs）。
// 積木編輯器 UMG 重做的 Phase 0 前置——Palette 沒有這份資料表就沒有東西可以列。
// 純靜態資料，不依賴任何 UI/UObject，AbilitySystem plugin 內（不是 SkillCreatorUI，
// 因為 FSpellSlotSync 已經需要查這份表，UI plugin 不能反過來被低層 plugin 依賴）。
class ABILITYSYSTEM_API FTotemLibrary
{
public:
    // 29 個技能因子（Godot UI/TotemLibrary.cs:15-65；標頭註解寫「27種」是 Godot 自己的過時註解，
    // 實際數量以 new TotemData 出現次數核實為 29）
    static const TArray<FTotemData>& AllTotems();

    // 117 個刻印（Godot UI/TotemLibrary.cs:71-213；標頭註解寫「90種」同樣是過時註解，
    // 實際數量以 new EngraveData 出現次數核實為 117）
    static const TArray<FEngraveData>& AllEngravings();

    // 哪些 Action 刻印雙擊後要進入容器效果編輯（Godot TotemLibrary.cs:219-229）
    static const TSet<FName>& ContainerActionIds();

    // 技能因子 Id → 預設自動插入的 Action 刻印 Id（Godot TotemLibrary.cs:235-266）
    static const TMap<FName, FName>& DefaultActionEngraveId();

    static const FTotemData*   FindTotem(FName Id);
    static const FEngraveData* FindEngraving(FName Id);
};
