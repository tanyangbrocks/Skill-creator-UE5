#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemId.h"
#include "UCraftingPanelWidget.generated.h"

class UInventoryComponent;
class UScrollBox;
class UWrapBox;
class UBorder;
class UTextBlock;
class UCraftingCardWidget;

// 加工選單面板（docs/plan-item-crafting-system.md §八 UI 設計）：常駐顯示在畫面左側
// （比照生存條 HUD 的呈現方式，獨立面板常駐可見，不是按鍵開關），5 欄圖卡網格，
// 超過可視範圍時用 UScrollBox 捲動（簡化原規格的「選單按鈕彈出側欄」機制，效果等價）。
UCLASS()
class SKILLCREATORRUNTIME_API UCraftingPanelWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // 每幀（或物品欄變動時）呼叫；內部比對可加工清單是否改變，沒變就不重建 widget tree
    void RefreshCraftable(UInventoryComponent* InPlayerInv, UInventoryComponent* InChestInv, bool bHasWorkbench);

    // 供 UCraftingHintCardWidget 讀取可加工物總數
    int32 GetCraftableCount() const { return CachedCraftable.Num(); }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geo, float Delta) override;

private:
    TObjectPtr<UWrapBox> Grid;
    TObjectPtr<UBorder>    TooltipPanel;
    TObjectPtr<UTextBlock> TooltipText;

    TArray<TObjectPtr<UCraftingCardWidget>> Cards;
    TArray<EItemId> CachedCraftable;
    bool bLastHasWorkbench = false;

    TWeakObjectPtr<UInventoryComponent> PlayerInv;
    TWeakObjectPtr<UInventoryComponent> ChestInv;

    void RebuildCards(const TArray<EItemId>& Craftable);
    void OnCardClicked(EItemId Output);
    void ConsumeFromPools(EItemId Id, int32 Count);
};
