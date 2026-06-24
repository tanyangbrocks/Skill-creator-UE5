#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemId.h"
#include "UEquipmentSlotWidget.generated.h"

class UBorder;
class UTextBlock;
class UButton;

// 單一裝備欄位格子（圖示+名稱+可點擊卸下），UEquipmentWidget 依
// FEquipmentSlotRegistry::GetAll() 動態建立 N 個實例，取代原本固定 3 槽寫法。
// 點擊用 TFunction（同 UParamControlWidgets 系列既有慣例：dynamic delegate 不支援
// AddLambda，每個實例自己的 UFUNCTION 轉呼叫 Setup() 存下來的 TFunction）。
UCLASS()
class SKILLCREATORRUNTIME_API UEquipmentSlotWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Setup(FName InSlotId, const FText& TypeLabel, TFunction<void(FName)> InOnClicked);
    void Refresh(EItemId Equipped);

protected:
    virtual void NativeOnInitialized() override;

private:
    FName SlotId;
    TObjectPtr<UBorder>    IconBorder;
    TObjectPtr<UTextBlock> NameLabel;
    TObjectPtr<UTextBlock> TypeLabelText;
    FText PendingTypeLabel;
    TFunction<void(FName)> OnClicked;

    UFUNCTION() void HandleClicked();
};
