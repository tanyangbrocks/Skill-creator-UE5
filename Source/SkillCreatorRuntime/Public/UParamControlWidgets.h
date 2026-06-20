#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UParamControlWidgets.generated.h"

class UEditableTextBox;
class USpinBox;
class UCheckBox;
class UComboBoxString;
class UTextBlock;
class USizeBox;

// Phase 4：積木參數控制項共用包裝（對應 Godot ScratchCanvas.cs:311-388 的
// SmallEdit/SmallSpin/SmallDrop/CheckBox 工廠）。UMG 的這些控制項事件都是
// dynamic multicast delegate（不支援 AddLambda），所以每個包裝只綁一個固定
// UFUNCTION，內部再轉呼叫 Setup() 時傳入的 TFunction callback——跟
// UPaletteItemWidget/UCardDragHandleWidget 用的同一招。

// 文字輸入（對應 Godot SmallEdit）
UCLASS()
class SKILLCREATORRUNTIME_API UParamTextEditWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Setup(const FString& InitialText, const FText& Hint, float Width, TFunction<void(const FString&)> InOnCommit);
protected:
    virtual void NativeConstruct() override;
private:
    TObjectPtr<UEditableTextBox> Box;
    TObjectPtr<USizeBox> SizeBox;
    TFunction<void(const FString&)> OnCommit;
    UFUNCTION() void HandleCommitted(const FText& Text, ETextCommit::Type CommitMethod);
};

// 數值微調（對應 Godot SmallSpin）
UCLASS()
class SKILLCREATORRUNTIME_API UParamSpinWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Setup(float InitialValue, float Min, float Max, float Step, float Width, TFunction<void(float)> InOnChanged);
protected:
    virtual void NativeConstruct() override;
private:
    TObjectPtr<USpinBox> Box;
    TFunction<void(float)> OnChanged;
    UFUNCTION() void HandleValueChanged(float NewValue);
};

// 下拉選單（對應 Godot SmallDrop；Values 是實際存的值，Labels 是顯示文字）
UCLASS()
class SKILLCREATORRUNTIME_API UParamDropdownWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Setup(const TArray<FString>& Values, const TArray<FString>& Labels, const FString& CurrentValue,
               float Width, TFunction<void(const FString&)> InOnChanged);
protected:
    virtual void NativeConstruct() override;
private:
    TObjectPtr<UComboBoxString> Box;
    TArray<FString> ValueList;
    TFunction<void(const FString&)> OnChanged;
    UFUNCTION() void HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
};

// 布林勾選（對應 Godot CheckBox）
UCLASS()
class SKILLCREATORRUNTIME_API UParamCheckboxWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Setup(bool bInitialValue, const FText& Label, TFunction<void(bool)> InOnChanged);
protected:
    virtual void NativeConstruct() override;
private:
    TObjectPtr<UCheckBox> Box;
    TFunction<void(bool)> OnChanged;
    UFUNCTION() void HandleStateChanged(bool bIsChecked);
};

// 小型灰字標籤（對應 Godot TinyLbl，純顯示用）
UCLASS()
class SKILLCREATORRUNTIME_API UParamTinyLabelWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Setup(const FText& InText);
protected:
    virtual void NativeConstruct() override;
private:
    TObjectPtr<UTextBlock> Text;
};
