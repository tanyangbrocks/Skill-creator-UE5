#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemStack.h"
#include "UInventoryWidget.generated.h"

class UBorder;
class UTextBlock;
class UCanvasPanel;
class UInventoryComponent;

// 物品欄面板（Z 鍵開關）：10 熱鍵欄 + 20 背包格，支援拖曳交換
UCLASS()
class SKILLCREATORRUNTIME_API UInventoryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    static constexpr int32 TotalSlots  = 30;
    static constexpr int32 HotbarSlots = 10;
    static constexpr int32 Cols        = 5;

    // 外部訂閱：交換請求（HUD 負責實際 swap）
    TDelegate<void(int32, int32)> OnSlotSwapRequested;
    TDelegate<void(int32)>        OnSlotEquipRequested;

    // 2026-06-23：跨欄拖曳（UChestWidget 用，docs/plan-item-crafting-system.md §六 UI 設計）。
    // SetPairedWidget() 設定後，drop 在自己範圍外、但落在 PairedWidget 範圍內時觸發。
    TDelegate<void(int32 SrcSlot, int32 DstSlot)> OnCrossSwapRequested;
    void SetPairedWidget(UInventoryWidget* Other) { PairedWidget = Other; }

    void Refresh(const UInventoryComponent* Inv);

    // 顯示懸停提示（由 ASkillCreatorHUD 控制）
    void ShowFloatTooltip(const FString& Text, FVector2D ScreenPos);
    void HideFloatTooltip();

    // 公開給 PairedWidget 跨欄查詢用（原本只給自己內部用）
    int32 GetSlotUnderMouse() const;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geo, float Delta) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Ev) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& Ev) override;

private:
    // 30 格
    TArray<TObjectPtr<UBorder>>    SlotBorders;
    TArray<TObjectPtr<UBorder>>    IconBorders;
    TArray<TObjectPtr<UTextBlock>> CountLabels;

    // 拖曳狀態
    int32 DragSrcSlot = -1;
    TObjectPtr<UBorder>    DragFloatIcon = nullptr;

    // 懸停 tooltip（本地快速顯示）
    TObjectPtr<UBorder>    TooltipPanel = nullptr;
    TObjectPtr<UTextBlock> TooltipText  = nullptr;

    // 快取上次渲染的資料
    TArray<FItemStack> CachedSlots;
    int32 CachedActiveIdx = 0;

    TWeakObjectPtr<UInventoryWidget> PairedWidget;

    FLinearColor GetItemColor(EItemId Id) const;
    void RefreshSlot(int32 Idx);
};
