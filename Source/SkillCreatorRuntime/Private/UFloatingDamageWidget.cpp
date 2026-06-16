#include "UFloatingDamageWidget.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

void UFloatingDamageWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildLayout();
}

void UFloatingDamageWidget::BuildLayout()
{
    if (!WidgetTree) return;

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    DamageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DamageText"));
    DamageText->SetText(FText::FromString(TEXT("0")));
    DamageText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

    FSlateFontInfo Font = DamageText->GetFont();
    Font.Size = 24;
    DamageText->SetFont(Font);

    UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(DamageText);
    if (CanvasSlot)
    {
        CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
        CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        CanvasSlot->SetAutoSize(true);
    }
}

void UFloatingDamageWidget::SetDamageText(float Amount, bool bIsCrit)
{
    if (!DamageText) return;
    FString Str = bIsCrit
        ? FString::Printf(TEXT("%.0f!"), Amount)
        : FString::Printf(TEXT("%.0f"),  Amount);
    DamageText->SetText(FText::FromString(Str));

    if (bIsCrit)
    {
        FSlateFontInfo Font = DamageText->GetFont();
        Font.Size = 32;
        DamageText->SetFont(Font);
    }
}

void UFloatingDamageWidget::SetDamageColor(FLinearColor Color)
{
    if (DamageText)
        DamageText->SetColorAndOpacity(FSlateColor(Color));
}

void UFloatingDamageWidget::SetAlpha(float Alpha)
{
    if (DamageText)
    {
        FLinearColor C = DamageText->GetColorAndOpacity().GetSpecifiedColor();
        C.A = FMath::Clamp(Alpha, 0.f, 1.f);
        DamageText->SetColorAndOpacity(FSlateColor(C));
    }
}
