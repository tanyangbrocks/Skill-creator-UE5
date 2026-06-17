#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EquipmentSlotType.h"
#include "UEquipmentWidget.generated.h"

class UBorder;
class UTextBlock;
class UEquipmentComponent;

// 裝備欄面板（X 鍵開關）：武器 / 防具 / 飾品 3 槽
UCLASS()
class SKILLCREATORRUNTIME_API UEquipmentWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // 請求卸下裝備（HUD 負責實際卸下）
    TDelegate<void(EEquipmentSlotType)> OnUnequipRequested;

    void Refresh(const UEquipmentComponent* Equip);

protected:
    virtual void NativeConstruct() override;

private:
    TObjectPtr<UBorder>    SlotBorders[3];
    TObjectPtr<UBorder>    IconBorders[3];
    TObjectPtr<UTextBlock> NameLabels[3];

    TObjectPtr<UBorder>    TooltipPanel = nullptr;
    TObjectPtr<UTextBlock> TooltipText  = nullptr;

    FLinearColor GetItemColor(EItemId Id) const;

    UFUNCTION() void OnSlot0Clicked();
    UFUNCTION() void OnSlot1Clicked();
    UFUNCTION() void OnSlot2Clicked();
};
