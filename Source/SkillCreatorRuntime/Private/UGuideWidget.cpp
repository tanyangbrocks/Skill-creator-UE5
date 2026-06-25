#include "UGuideWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

void UGuideWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Root = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Root->SetBrushColor(FLinearColor(0.93f, 0.96f, 1.0f, 1.f));
    WidgetTree->RootWidget = Root;
    auto* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Lbl->SetText(FText::FromString(TEXT("引導（施工中）")));
    Lbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f)));
    Root->SetContent(Lbl);
}
