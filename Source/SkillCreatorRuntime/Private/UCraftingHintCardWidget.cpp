#include "UCraftingHintCardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/TextBlock.h"
#include "SlateBrushHelpers.h"

void UCraftingHintCardWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    constexpr float W = 44.f, H = 44.f;

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Card->SetBrush(MakeSolidBrush(FLinearColor(0.05f, 0.12f, 0.06f, 0.85f)));
    Card->SetPadding(FMargin(0.f));
    Root->AddChild(Card);
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Card->Slot))
    {
        // 左側偏上：HP 圓形（約 120px 高）下方，距頂 140px
        S->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
        S->SetPosition(FVector2D(10.f, 140.f));
        S->SetSize(FVector2D(W, H));
        S->SetAlignment(FVector2D::ZeroVector);
    }

    // 內層 Canvas 承載 CountLabel（UBorder 只允許單一子節點）
    UCanvasPanel* Inner = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    Card->AddChild(Inner);
    if (UBorderSlot* BS = Cast<UBorderSlot>(Inner->Slot))
    {
        BS->SetHorizontalAlignment(HAlign_Fill);
        BS->SetVerticalAlignment(VAlign_Fill);
    }

    // 計數器標籤（右下角）
    CountLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    {
        FSlateFontInfo F = CountLabel->GetFont();
        F.Size = 10;
        CountLabel->SetFont(F);
    }
    CountLabel->SetText(FText::FromString(TEXT("0")));
    CountLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.92f, 0.30f)));
    Inner->AddChild(CountLabel);
    if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(CountLabel->Slot))
    {
        CS->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f));
        CS->SetAlignment(FVector2D(1.f, 1.f));
        CS->SetAutoSize(true);
        CS->SetPosition(FVector2D(-3.f, -3.f));
    }
}

void UCraftingHintCardWidget::UpdateCount(int32 Count)
{
    if (CountLabel) CountLabel->SetText(FText::AsNumber(Count));
}
