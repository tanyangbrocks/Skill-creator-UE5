#include "UParamControlWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"
#include "Components/SpinBox.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"

// ── UParamTextEditWidget ─────────────────────────────────────────────

void UParamTextEditWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    WidgetTree->RootWidget = Size;
    Box = WidgetTree->ConstructWidget<UEditableTextBox>();
    Size->SetContent(Box);
    Box->OnTextCommitted.AddDynamic(this, &UParamTextEditWidget::HandleCommitted);
    SizeBox = Size;
}

void UParamTextEditWidget::Setup(const FString& InitialText, const FText& Hint, float Width,
                                  TFunction<void(const FString&)> InOnCommit)
{
    OnCommit = MoveTemp(InOnCommit);
    if (!Box) return;
    Box->SetText(FText::FromString(InitialText));
    Box->SetHintText(Hint);
    if (SizeBox) SizeBox->SetWidthOverride(Width);
}

void UParamTextEditWidget::HandleCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    if (OnCommit) OnCommit(Text.ToString());
}

// ── UParamSpinWidget ─────────────────────────────────────────────────

void UParamSpinWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    Box = WidgetTree->ConstructWidget<USpinBox>();
    WidgetTree->RootWidget = Box;
    Box->OnValueChanged.AddDynamic(this, &UParamSpinWidget::HandleValueChanged);
}

void UParamSpinWidget::Setup(float InitialValue, float Min, float Max, float Step, float Width,
                              TFunction<void(float)> InOnChanged)
{
    OnChanged = MoveTemp(InOnChanged);
    if (!Box) return;
    Box->SetMinValue(Min);
    Box->SetMaxValue(Max);
    Box->SetMinSliderValue(Min);
    Box->SetMaxSliderValue(Max);
    Box->SetDelta(Step);
    Box->SetValue(InitialValue);
    Box->SetMinDesiredWidth(Width);
}

void UParamSpinWidget::HandleValueChanged(float NewValue)
{
    if (OnChanged) OnChanged(NewValue);
}

// ── UParamDropdownWidget ─────────────────────────────────────────────

void UParamDropdownWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    Box = WidgetTree->ConstructWidget<UComboBoxString>();
    WidgetTree->RootWidget = Box;
    Box->OnSelectionChanged.AddDynamic(this, &UParamDropdownWidget::HandleSelectionChanged);
}

void UParamDropdownWidget::Setup(const TArray<FString>& Values, const TArray<FString>& Labels,
                                  const FString& CurrentValue, float Width,
                                  TFunction<void(const FString&)> InOnChanged)
{
    OnChanged = MoveTemp(InOnChanged);
    ValueList = Values;
    if (!Box) return;

    Box->ClearOptions();
    for (const FString& L : Labels)
        Box->AddOption(L);

    int32 CurIdx = Values.IndexOfByKey(CurrentValue);
    if (CurIdx != INDEX_NONE && Labels.IsValidIndex(CurIdx))
        Box->SetSelectedOption(Labels[CurIdx]);
    else if (Labels.Num() > 0)
        Box->SetSelectedOption(Labels[0]);
}

void UParamDropdownWidget::HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (!Box || !OnChanged) return;
    const int32 Idx = Box->GetOptionCount() > 0 ? Box->FindOptionIndex(SelectedItem) : INDEX_NONE;
    if (ValueList.IsValidIndex(Idx))
        OnChanged(ValueList[Idx]);
}

// ── UParamCheckboxWidget ─────────────────────────────────────────────

void UParamCheckboxWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    Box = WidgetTree->ConstructWidget<UCheckBox>();
    WidgetTree->RootWidget = Box;
    Box->OnCheckStateChanged.AddDynamic(this, &UParamCheckboxWidget::HandleStateChanged);
}

void UParamCheckboxWidget::Setup(bool bInitialValue, const FText& Label, TFunction<void(bool)> InOnChanged)
{
    OnChanged = MoveTemp(InOnChanged);
    if (!Box) return;
    Box->SetIsChecked(bInitialValue);
    UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
    Txt->SetText(Label);
    Box->SetContent(Txt);
}

void UParamCheckboxWidget::HandleStateChanged(bool bIsChecked)
{
    if (OnChanged) OnChanged(bIsChecked);
}

// ── UParamTinyLabelWidget ─────────────────────────────────────────────

void UParamTinyLabelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    Text = WidgetTree->ConstructWidget<UTextBlock>();
    WidgetTree->RootWidget = Text;
    Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.58f, 0.58f, 0.62f)));
}

void UParamTinyLabelWidget::Setup(const FText& InText)
{
    if (Text) Text->SetText(InText);
}
