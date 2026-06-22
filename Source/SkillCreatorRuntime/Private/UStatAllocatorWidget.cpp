#include "UStatAllocatorWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"

void UStatAllocatorWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    UHorizontalBox* Root = WidgetTree->ConstructWidget<UHorizontalBox>();
    WidgetTree->RootWidget = Root;

    LabelText = WidgetTree->ConstructWidget<UTextBlock>();
    LabelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.90f)));
    {
        FSlateFontInfo F = LabelText->GetFont();
        F.Size = 22;
        LabelText->SetFont(F);
    }
    USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>();
    LabelBox->SetWidthOverride(90.f);
    LabelBox->SetContent(LabelText);
    if (UHorizontalBoxSlot* S = Root->AddChildToHorizontalBox(LabelBox))
        S->SetVerticalAlignment(VAlign_Center);

    MinusBtn = WidgetTree->ConstructWidget<UButton>();
    MinusBtn->SetBackgroundColor(FLinearColor(0.25f, 0.18f, 0.18f, 1.f));
    UTextBlock* MinusTxt = WidgetTree->ConstructWidget<UTextBlock>();
    MinusTxt->SetText(FText::FromString(TEXT("−")));
    {
        FSlateFontInfo F = MinusTxt->GetFont();
        F.Size = 22;
        MinusTxt->SetFont(F);
    }
    MinusTxt->SetJustification(ETextJustify::Center);
    MinusBtn->AddChild(MinusTxt);
    MinusBtn->OnClicked.AddDynamic(this, &UStatAllocatorWidget::HandleMinusClicked);
    USizeBox* MinusBox = WidgetTree->ConstructWidget<USizeBox>();
    MinusBox->SetWidthOverride(44.f);
    MinusBox->SetHeightOverride(44.f);
    MinusBox->SetContent(MinusBtn);
    if (UHorizontalBoxSlot* S = Root->AddChildToHorizontalBox(MinusBox))
        S->SetPadding(FMargin(10.f, 0.f));

    ValueInput = WidgetTree->ConstructWidget<UEditableTextBox>();
    ValueInput->SetJustification(ETextJustify::Center);
    {
        FSlateFontInfo F = ValueInput->WidgetStyle.TextStyle.Font;
        F.Size = 22;
        ValueInput->WidgetStyle.TextStyle.Font = F;
    }
    ValueInput->OnTextCommitted.AddDynamic(this, &UStatAllocatorWidget::HandleValueCommitted);
    USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>();
    ValueBox->SetWidthOverride(64.f);
    ValueBox->SetHeightOverride(40.f);
    ValueBox->SetContent(ValueInput);
    if (UHorizontalBoxSlot* S = Root->AddChildToHorizontalBox(ValueBox))
        S->SetVerticalAlignment(VAlign_Center);

    PlusBtn = WidgetTree->ConstructWidget<UButton>();
    PlusBtn->SetBackgroundColor(FLinearColor(0.18f, 0.25f, 0.18f, 1.f));
    UTextBlock* PlusTxt = WidgetTree->ConstructWidget<UTextBlock>();
    PlusTxt->SetText(FText::FromString(TEXT("+")));
    {
        FSlateFontInfo F = PlusTxt->GetFont();
        F.Size = 22;
        PlusTxt->SetFont(F);
    }
    PlusTxt->SetJustification(ETextJustify::Center);
    PlusBtn->AddChild(PlusTxt);
    PlusBtn->OnClicked.AddDynamic(this, &UStatAllocatorWidget::HandlePlusClicked);
    USizeBox* PlusBox = WidgetTree->ConstructWidget<USizeBox>();
    PlusBox->SetWidthOverride(44.f);
    PlusBox->SetHeightOverride(44.f);
    PlusBox->SetContent(PlusBtn);
    if (UHorizontalBoxSlot* S = Root->AddChildToHorizontalBox(PlusBox))
        S->SetPadding(FMargin(10.f, 0.f));
}

// Setup() 在 WidgetTree->ConstructWidget<UStatAllocatorWidget>() 之後立即被呼叫端呼叫——
// ConstructWidget<T>() 對 UUserWidget 子類會同步走 CreateWidget()->Initialize()->
// NativeOnInitialized()（見 WidgetTree.h ConstructWidget 實作），所以這裡 LabelText/ValueText
// 保證已經存在，可以直接設值，不需要像 UCardDragHandleWidget 那樣把參數存起來等之後補畫。
void UStatAllocatorWidget::Setup(const FText& InLabel, int32 InitialValue,
    TFunction<void(int32)> InOnAdjust, TFunction<void(int32)> InOnSetValue)
{
    OnAdjust   = MoveTemp(InOnAdjust);
    OnSetValue = MoveTemp(InOnSetValue);
    LastValue  = InitialValue;
    if (LabelText)  LabelText->SetText(InLabel);
    if (ValueInput) ValueInput->SetText(FText::AsNumber(InitialValue));
}

void UStatAllocatorWidget::UpdateDisplay(int32 NewValue, bool bCanDecrement, bool bCanIncrement)
{
    LastValue = NewValue;
    if (ValueInput) ValueInput->SetText(FText::AsNumber(NewValue));
    if (MinusBtn)   MinusBtn->SetIsEnabled(bCanDecrement);
    if (PlusBtn)    PlusBtn->SetIsEnabled(bCanIncrement);
}

void UStatAllocatorWidget::HandleMinusClicked()
{
    if (OnAdjust) OnAdjust(-1);
}

void UStatAllocatorWidget::HandlePlusClicked()
{
    if (OnAdjust) OnAdjust(1);
}

void UStatAllocatorWidget::HandleValueCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    // 打字輸入非數字（或取消編輯）時直接還原成目前的值，不呼叫 OnSetValue——呼叫端
    // （UPlayerCreateWidget::SetStatValue）負責 clamp 到合法範圍並透過 UpdateDisplay()
    // 把最終結果寫回來，這裡不需要自己做範圍檢查。
    int32 Parsed = 0;
    const bool bValid = LexTryParseString(Parsed, *Text.ToString());
    if (bValid && OnSetValue)
        OnSetValue(Parsed);
    else if (ValueInput)
        ValueInput->SetText(FText::AsNumber(LastValue));
}
