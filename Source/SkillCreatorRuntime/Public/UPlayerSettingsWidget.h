#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UPlayerSettingsWidget.generated.h"

class UCheckBox;
class UScrollBox;
class UVerticalBox;
class UInputSettingsWidget;

// 設定全版介面（右上角「設定」按鈕觸發）。
// 包含：操作細節（長按連放 + 完美移除放置）+ 鍵位設定（UInputSettingsWidget）。
// Stage 4 完整實作；Stage 2~3 為空白佔位。
UCLASS()
class SKILLCREATORRUNTIME_API UPlayerSettingsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // ASkillCreatorHUD 呼叫，同步當前設定值到 Checkbox
    void SyncState(bool bHold, bool bPerfect);

    TDelegate<void(bool)> OnHoldToPlaceChanged;
    TDelegate<void(bool)> OnPerfectRemoveChanged;

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY() TObjectPtr<UCheckBox>            HoldCheck      = nullptr;
    UPROPERTY() TObjectPtr<UCheckBox>            PerfectCheck   = nullptr;
    UPROPERTY() TObjectPtr<UInputSettingsWidget> KeybindSection = nullptr;

    UFUNCTION() void OnHoldToggled(bool b);
    UFUNCTION() void OnPerfectToggled(bool b);
};
