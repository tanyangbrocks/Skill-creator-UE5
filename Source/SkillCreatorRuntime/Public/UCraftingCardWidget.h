#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemId.h"
#include "UCraftingCardWidget.generated.h"

class UBorder;
class UButton;

// 單一加工物方形圖卡（docs/plan-item-crafting-system.md §八 UI 設計）。
// 點擊用 TFunction（同 UEquipmentSlotWidget/UParamControlWidgets 系列既有慣例）。
UCLASS()
class SKILLCREATORRUNTIME_API UCraftingCardWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Setup(EItemId InItemId, TFunction<void(EItemId)> InOnClicked);

    // Hover 用：外部（UCraftingPanelWidget::NativeTick）偵測滑鼠是否在這張卡上
    bool IsUnderMouse() const;
    EItemId GetItemId() const { return ItemId; }

protected:
    virtual void NativeOnInitialized() override;

private:
    EItemId ItemId = EItemId::None;
    TObjectPtr<UBorder> IconBorder;
    TFunction<void(EItemId)> OnClicked;

    UFUNCTION() void HandleClicked();
};
