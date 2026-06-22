#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "USettingsWidget.generated.h"

class UCheckBox;

// 設定面板（B 鍵開關）：長按連放 + 完美移除放置物件 toggle
UCLASS()
class SKILLCREATORRUNTIME_API USettingsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    bool bHoldToPlace   = false;
    bool bPerfectRemove = false;

    // 外部訂閱：設定值改變時收到通知
    TDelegate<void(bool)> OnHoldToPlaceChanged;
    TDelegate<void(bool)> OnPerfectRemoveChanged;
    TDelegate<void()>     OnCloseRequested;

    // HUD 開啟面板前呼叫，同步最新值到 CheckBox
    void SyncState(bool bHold, bool bPerfect);

protected:
    virtual void NativeOnInitialized() override;

private:
    TObjectPtr<UCheckBox> HoldCheck    = nullptr;
    TObjectPtr<UCheckBox> PerfectCheck = nullptr;

    UFUNCTION() void OnHoldToggled(bool bIsChecked);
    UFUNCTION() void OnPerfectToggled(bool bIsChecked);
    UFUNCTION() void OnCloseClicked();
};
