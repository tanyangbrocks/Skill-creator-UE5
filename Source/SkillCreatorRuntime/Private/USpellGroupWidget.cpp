#include "USpellGroupWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"

static void PinCenterGroup(UWidget* W, FVector2D HalfSize)
{
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(W->Slot))
    {
        S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
        S->SetPosition(-HalfSize);
        S->SetSize(HalfSize * 2.f);
        S->SetAlignment(FVector2D::ZeroVector);
    }
}

void USpellGroupWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    UBorder* ClickBlock = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Root->AddChild(ClickBlock);
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(ClickBlock->Slot))
        S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    ClickBlock->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.2f));

    // Panel 240×auto
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
    Panel->SetBrushColor(FLinearColor(0.10f, 0.10f, 0.16f, 0.95f));
    Panel->SetPadding(FMargin(14.f, 10.f));
    Root->AddChild(Panel);
    PinCenterGroup(Panel, FVector2D(120.f, 110.f));

    UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Panel->AddChild(VBox);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Title->SetText(FText::FromString(TEXT("切換技能組（V 關閉）")));
    {
        FSlateFontInfo F = Title->GetFont();
        F.Size = 12;
        Title->SetFont(F);
    }
    Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.85f, 1.0f)));
    Title->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(Title))
    {
        VS->SetHorizontalAlignment(HAlign_Fill);
        VS->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
    }

    for (int32 i = 0; i < MaxGroups; ++i)
    {
        UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());

        UTextBlock* BtnTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        BtnTxt->SetText(FText::FromString(FString::Printf(TEXT("技能組 %d"), i + 1)));
        {
            FSlateFontInfo F = BtnTxt->GetFont();
            F.Size = 13;
            BtnTxt->SetFont(F);
        }
        BtnTxt->SetJustification(ETextJustify::Center);
        Btn->AddChild(BtnTxt);

        if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(Btn))
        {
            VS->SetHorizontalAlignment(HAlign_Fill);
            VS->SetPadding(FMargin(0.f, 2.f));
        }
        GroupButtons[i]     = Btn;
        GroupButtonTexts[i] = BtnTxt;
    }

    GroupButtons[0]->OnClicked.AddDynamic(this, &USpellGroupWidget::OnGroup0Clicked);
    GroupButtons[1]->OnClicked.AddDynamic(this, &USpellGroupWidget::OnGroup1Clicked);
    GroupButtons[2]->OnClicked.AddDynamic(this, &USpellGroupWidget::OnGroup2Clicked);
    GroupButtons[3]->OnClicked.AddDynamic(this, &USpellGroupWidget::OnGroup3Clicked);
    GroupButtons[4]->OnClicked.AddDynamic(this, &USpellGroupWidget::OnGroup4Clicked);

    RefreshButtonHighlight();
}

void USpellGroupWidget::SetActiveGroup(int32 Idx)
{
    ActiveGroupIndex = Idx;
    RefreshButtonHighlight();
}

void USpellGroupWidget::SelectGroup(int32 Idx)
{
    ActiveGroupIndex = Idx;
    RefreshButtonHighlight();
    OnGroupSelected.ExecuteIfBound(Idx);
    OnCloseRequested.ExecuteIfBound();
}

void USpellGroupWidget::RefreshButtonHighlight()
{
    for (int32 i = 0; i < MaxGroups; ++i)
    {
        if (!GroupButtonTexts[i]) continue;
        bool bActive = (i == ActiveGroupIndex);
        GroupButtonTexts[i]->SetColorAndOpacity(FSlateColor(
            bActive ? FLinearColor(0.40f, 0.85f, 1.00f) : FLinearColor(0.85f, 0.85f, 0.90f)));
    }
}

void USpellGroupWidget::OnGroup0Clicked() { SelectGroup(0); }
void USpellGroupWidget::OnGroup1Clicked() { SelectGroup(1); }
void USpellGroupWidget::OnGroup2Clicked() { SelectGroup(2); }
void USpellGroupWidget::OnGroup3Clicked() { SelectGroup(3); }
void USpellGroupWidget::OnGroup4Clicked() { SelectGroup(4); }
