#pragma once
#include "CoreMinimal.h"
#include "BlockType.h"

class UWidget;
class UHorizontalBox;
struct FBlockNode;

// Phase 4：積木參數 UI 集中表（對應 Godot ScratchCanvas.cs:474-850 的 `_descs` 字典）。
// 取代 Phase 2/3 用的「每類別一色 + 英文 UENUM 名稱」簡化版——這裡是逐個 EBlockType
// 精確對照 Godot 顏色常數 + 中文名稱 + 參數控制項。
//
// Totem/Engraving 不在這個表裡：它們的顏色/名稱取決於積木實例攜帶的子型別資料
// （FTotemBlockArgs.TotemType / FEngravingBlockArgs.EngraveId 查 FTotemLibrary），
// 在 UBlockCardWidget 裡已有專門處理，不適合塞進這種「純看 EBlockType」的靜態表。
struct FBlockUIDescriptor
{
    FLinearColor Color = FLinearColor::White;
    FText        DisplayName;

    // 把這個積木的參數控制項塞進 Row；Block 是要編輯的積木實例本身；
    // OnStructuralChange 只在「條件類型」這種會改變要顯示哪些子控制項的情況才呼叫
    // （觸發上層 RebuildList() 重新整理這一列），純粹改值不需要呼叫。
    TFunction<void(UWidget* WidgetContext, UHorizontalBox* Row, FBlockNode& Block,
                   const TFunction<void()>& OnStructuralChange)> BuildParamsUI;
};

class SKILLCREATORRUNTIME_API FBlockUIRegistry
{
public:
    // 找不到時回傳一個只有預設色的 fallback（不應該發生，84 種型別都已登記）
    static const FBlockUIDescriptor& Get(EBlockType Type);
};
