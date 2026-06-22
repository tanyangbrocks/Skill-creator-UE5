#include "USettingsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Border.h"
#include "Components/Spacer.h"

static void PinToCenter(UWidget* W, FVector2D HalfSize)
{
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(W->Slot))
    {
        S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
        S->SetPosition(-HalfSize);
        S->SetSize(HalfSize * 2.f);
        S->SetAlignment(FVector2D::ZeroVector);
    }
}

static UTextBlock* MakeLabel(UWidgetTree* WT, const FString& Txt, float FontSz, FLinearColor Col)
{
    UTextBlock* TB = WT->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TB->SetText(FText::FromString(Txt));
    FSlateFontInfo Font = TB->GetFont();
    Font.Size = (int32)FontSz;
    TB->SetFont(Font);
    TB->SetColorAndOpacity(FSlateColor(Col));
    return TB;
}

static UButton* MakeButton(UWidgetTree* WT, const FString& Txt, float FontSz)
{
    UButton* Btn = WT->ConstructWidget<UButton>(UButton::StaticClass());
    UTextBlock* BtnTxt = WT->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    BtnTxt->SetText(FText::FromString(Txt));
    FSlateFontInfo Font = BtnTxt->GetFont();
    Font.Size = (int32)FontSz;
    BtnTxt->SetFont(Font);
    BtnTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.9f, 0.9f)));
    BtnTxt->SetJustification(ETextJustify::Center);
    Btn->AddChild(BtnTxt);
    return Btn;
}

void USettingsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // 全螢幕點擊攔截（透明）
    UBorder* ClickBlock = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ClickBlock"));
    Root->AddChild(ClickBlock);
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(ClickBlock->Slot))
        S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    ClickBlock->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.3f));

    // 中央面板 320×240
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
    Panel->SetBrushColor(FLinearColor(0.10f, 0.10f, 0.14f, 0.96f));
    Panel->SetPadding(FMargin(20.f, 16.f));
    Root->AddChild(Panel);
    PinToCenter(Panel, FVector2D(160.f, 120.f));

    UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Panel->AddChild(VBox);

    // 標題
    UTextBlock* Title = MakeLabel(WidgetTree, TEXT("設定"), 16.f, FLinearColor(0.90f, 0.90f, 1.0f));
    Title->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(Title))
        VS->SetHorizontalAlignment(HAlign_Fill);

    // ── 長按連放 checkbox ──────────────────────────────────────
    UHorizontalBox* HoldRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(HoldRow))
        VS->SetHorizontalAlignment(HAlign_Fill);

    HoldCheck = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("HoldCheck"));
    HoldCheck->OnCheckStateChanged.AddDynamic(this, &USettingsWidget::OnHoldToggled);
    HoldRow->AddChildToHorizontalBox(HoldCheck);

    UTextBlock* HoldLabel = MakeLabel(WidgetTree, TEXT("長按連放"), 14.f, FLinearColor(0.85f, 0.85f, 0.85f));
    if (UHorizontalBoxSlot* HS = HoldRow->AddChildToHorizontalBox(HoldLabel))
        HS->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));

    UTextBlock* HoldHint = MakeLabel(WidgetTree, TEXT("（長按右鍵持續放置）"), 11.f, FLinearColor(0.5f, 0.5f, 0.55f));
    if (UHorizontalBoxSlot* HS = HoldRow->AddChildToHorizontalBox(HoldHint))
        HS->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));

    // ── 完美移除 checkbox ──────────────────────────────────────
    UHorizontalBox* PerfRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(PerfRow))
        VS->SetHorizontalAlignment(HAlign_Fill);

    PerfectCheck = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("PerfectCheck"));
    PerfectCheck->OnCheckStateChanged.AddDynamic(this, &USettingsWidget::OnPerfectToggled);
    PerfRow->AddChildToHorizontalBox(PerfectCheck);

    UTextBlock* PerfLabel = MakeLabel(WidgetTree, TEXT("完美移除放置物件"), 14.f, FLinearColor(0.85f, 0.85f, 0.85f));
    if (UHorizontalBoxSlot* HS = PerfRow->AddChildToHorizontalBox(PerfLabel))
        HS->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));

    UTextBlock* PerfHint = MakeLabel(WidgetTree, TEXT("（挖到自己放的物件時，整組移除）"), 10.f, FLinearColor(0.5f, 0.5f, 0.55f));
    if (UHorizontalBoxSlot* HS = PerfRow->AddChildToHorizontalBox(PerfHint))
        HS->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));

    // spacer
    USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
    if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(Spacer))
        VS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    // 關閉按鈕
    UButton* CloseBtn = MakeButton(WidgetTree, TEXT("關閉（B）"), 13.f);
    CloseBtn->OnClicked.AddDynamic(this, &USettingsWidget::OnCloseClicked);
    if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(CloseBtn))
        VS->SetHorizontalAlignment(HAlign_Fill);
}

void USettingsWidget::SyncState(bool bHold, bool bPerfect)
{
    bHoldToPlace   = bHold;
    bPerfectRemove = bPerfect;
    if (HoldCheck)    HoldCheck->SetIsChecked(bHold);
    if (PerfectCheck) PerfectCheck->SetIsChecked(bPerfect);
}

void USettingsWidget::OnHoldToggled(bool bIsChecked)
{
    bHoldToPlace = bIsChecked;
    OnHoldToPlaceChanged.ExecuteIfBound(bIsChecked);
}

void USettingsWidget::OnPerfectToggled(bool bIsChecked)
{
    bPerfectRemove = bIsChecked;
    OnPerfectRemoveChanged.ExecuteIfBound(bIsChecked);
}

void USettingsWidget::OnCloseClicked()
{
    OnCloseRequested.ExecuteIfBound();
}
