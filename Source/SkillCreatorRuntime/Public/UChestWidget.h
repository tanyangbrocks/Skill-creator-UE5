#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UChestWidget.generated.h"

class UInventoryWidget;
class UInventoryComponent;
class UCanvasPanel;

// 寶箱雙欄 UI（docs/plan-item-crafting-system.md §六 UI 設計）：水平並排「玩家背包」+
// 「寶箱背包」兩個獨立 UInventoryWidget 實例，支援跨欄拖曳（UInventoryWidget::SetPairedWidget）。
UCLASS()
class SKILLCREATORRUNTIME_API UChestWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // AChestActor::OpenChestUI() 開啟時呼叫；PlayerInv/ChestInv 缺一不可
    void Setup(UInventoryComponent* InPlayerInv, UInventoryComponent* InChestInv);
    void RefreshBoth();

protected:
    virtual void NativeOnInitialized() override;

private:
    TObjectPtr<UInventoryWidget> PlayerInvWidget;
    TObjectPtr<UInventoryWidget> ChestInvWidget;
    TWeakObjectPtr<UInventoryComponent> PlayerInv;
    TWeakObjectPtr<UInventoryComponent> ChestInv;
};
