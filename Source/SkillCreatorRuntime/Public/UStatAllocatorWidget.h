#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UStatAllocatorWidget.generated.h"

class UTextBlock;
class UButton;
class UEditableTextBox;

// W-10 步驟 6（基礎能力點分配）單列「− 數字輸入框 +」控制項，仿「人生重開模擬器」配置。
// 跟 UParamSpinWidget/UFlowCardWidget 同一套慣例：UButton::OnClicked 是 dynamic multicast，
// 不支援 AddLambda，所以用固定的 UFUNCTION 轉呼叫 Setup() 時捕捉到的 TFunction。
//
// 2026-06-22：數字欄位從 UTextBlock 改成 UEditableTextBox——使用者要求「每一項的數字都必須
// 是可以輸入、調整的欄位，不是純顯示數字」，直接打字輸入比只能按 −/+ 一次一點更快調整。
UCLASS()
class SKILLCREATORRUNTIME_API UStatAllocatorWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // InLabel：左側能力點名稱（體魄/肌力...）；InOnAdjust(+1/-1)：呼叫端決定是否真的允許這個增減
    // （剩餘創建能力點不足時呼叫端可以直接不做事）；InOnSetValue(NewValue)：使用者直接在輸入框
    // 打字確認後呼叫，呼叫端負責 clamp 到合法範圍（不能超過剩餘點數、不能小於 0）。
    // 兩者完成後都要呼叫端自己呼叫 UpdateDisplay()。
    void Setup(const FText& InLabel, int32 InitialValue,
        TFunction<void(int32 Delta)> InOnAdjust, TFunction<void(int32 NewValue)> InOnSetValue);

    // 呼叫端在自己的狀態改變後呼叫，更新顯示數字 + −/+ 按鈕是否可按
    void UpdateDisplay(int32 NewValue, bool bCanDecrement, bool bCanIncrement);

protected:
    virtual void NativeOnInitialized() override;

private:
    TFunction<void(int32)> OnAdjust;
    TFunction<void(int32)> OnSetValue;
    int32 LastValue = 0;

    UPROPERTY() TObjectPtr<UTextBlock>        LabelText  = nullptr;
    UPROPERTY() TObjectPtr<UEditableTextBox>  ValueInput = nullptr;
    UPROPERTY() TObjectPtr<UButton>           MinusBtn   = nullptr;
    UPROPERTY() TObjectPtr<UButton>           PlusBtn    = nullptr;

    UFUNCTION() void HandleMinusClicked();
    UFUNCTION() void HandlePlusClicked();
    UFUNCTION() void HandleValueCommitted(const FText& Text, ETextCommit::Type CommitMethod);
};
