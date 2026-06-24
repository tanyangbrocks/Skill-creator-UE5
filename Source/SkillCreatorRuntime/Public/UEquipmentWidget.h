#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemId.h"
#include "UEquipmentWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;
class UEquipmentComponent;
class UEquipmentSlotWidget;

// 裝備欄面板（X 鍵開關）。2026-06-23：欄位數量改成動態依 FEquipmentSlotRegistry::GetAll()
// 建立，不再寫死 3 槽——新增裝備欄只需要在登錄表加一行，這個類別完全不用改。
UCLASS()
class SKILLCREATORRUNTIME_API UEquipmentWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // 請求卸下裝備（HUD 負責實際卸下）；FName = FEquipmentSlotRegistry 的 Id
    TDelegate<void(FName)> OnUnequipRequested;

    void Refresh(const UEquipmentComponent* Equip);

protected:
    virtual void NativeOnInitialized() override;

private:
    TObjectPtr<UVerticalBox> SlotList;
    TArray<TObjectPtr<UEquipmentSlotWidget>> SlotWidgets;

    TObjectPtr<UBorder>    TooltipPanel = nullptr;
    TObjectPtr<UTextBlock> TooltipText  = nullptr;
};
