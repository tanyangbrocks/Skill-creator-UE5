#include "UCraftingCardWidget.h"
#include "ItemRegistry.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "SlateBrushHelpers.h"

void UCraftingCardWidget::Setup(EItemId InItemId, TFunction<void(EItemId)> InOnClicked)
{
    ItemId    = InItemId;
    OnClicked = MoveTemp(InOnClicked);
    if (IconBorder)
        IconBorder->SetBrush(MakeSolidBrush(FLinearColor(0.85f, 0.75f, 0.15f)));
}

void UCraftingCardWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    constexpr float CardS = 40.f;

    USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    SizeBox->SetWidthOverride(CardS);
    SizeBox->SetHeightOverride(CardS);
    WidgetTree->RootWidget = SizeBox;

    UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    Btn->OnClicked.AddDynamic(this, &UCraftingCardWidget::HandleClicked);
    SizeBox->AddChild(Btn);

    IconBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    IconBorder->SetBrush(MakeSolidBrush(FLinearColor(0.85f, 0.75f, 0.15f)));
    IconBorder->SetPadding(FMargin(2.f));
    Btn->AddChild(IconBorder);
}

bool UCraftingCardWidget::IsUnderMouse() const
{
    if (!GetCachedWidget().IsValid()) return false;
    return IsHovered();
}

void UCraftingCardWidget::HandleClicked()
{
    if (OnClicked) OnClicked(ItemId);
}
