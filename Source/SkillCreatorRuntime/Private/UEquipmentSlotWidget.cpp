#include "UEquipmentSlotWidget.h"
#include "ItemRegistry.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "SlateBrushHelpers.h"

void UEquipmentSlotWidget::Setup(FName InSlotId, const FText& TypeLabel, TFunction<void(FName)> InOnClicked)
{
    SlotId          = InSlotId;
    PendingTypeLabel = TypeLabel;
    OnClicked       = MoveTemp(InOnClicked);
    if (TypeLabelText)
        TypeLabelText->SetText(PendingTypeLabel);
}

void UEquipmentSlotWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    constexpr float IconS = 38.f, TypeW = 36.f, NameW = 90.f, MidGap = 4.f;

    UHorizontalBox* Root = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    TypeLabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TypeLabelText->SetText(PendingTypeLabel);
    {
        FSlateFontInfo F = TypeLabelText->GetFont(); F.Size = 11; TypeLabelText->SetFont(F);
    }
    TypeLabelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.60f, 0.70f, 0.65f)));
    if (UHorizontalBoxSlot* S = Root->AddChildToHorizontalBox(TypeLabelText))
    {
        FSlateChildSize TypeSize(ESlateSizeRule::Fill);
        TypeSize.Value = TypeW;
        S->SetSize(TypeSize);
        S->SetVerticalAlignment(VAlign_Center);
    }

    USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    IconSize->SetWidthOverride(IconS);
    IconSize->SetHeightOverride(IconS);
    if (UHorizontalBoxSlot* S = Root->AddChildToHorizontalBox(IconSize))
        S->SetPadding(FMargin(MidGap, 0.f, MidGap, 0.f));

    UButton* SlotBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    SlotBtn->OnClicked.AddDynamic(this, &UEquipmentSlotWidget::HandleClicked);
    IconSize->AddChild(SlotBtn);

    IconBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    IconBorder->SetBrush(MakeSolidBrush(FLinearColor(0.08f, 0.08f, 0.10f)));
    IconBorder->SetPadding(FMargin(0.f));
    SlotBtn->AddChild(IconBorder);

    NameLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    NameLabel->SetText(FText::FromString(TEXT("─")));
    {
        FSlateFontInfo F = NameLabel->GetFont(); F.Size = 11; NameLabel->SetFont(F);
    }
    NameLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.78f, 0.82f)));
    if (UHorizontalBoxSlot* S = Root->AddChildToHorizontalBox(NameLabel))
    {
        FSlateChildSize NameSize(ESlateSizeRule::Fill);
        NameSize.Value = NameW;
        S->SetSize(NameSize);
        S->SetVerticalAlignment(VAlign_Center);
    }
}

void UEquipmentSlotWidget::Refresh(EItemId Equipped)
{
    if (IconBorder)
    {
        const FLinearColor C = (Equipped == EItemId::None)
            ? FLinearColor(0.08f, 0.08f, 0.10f)
            : FLinearColor(0.85f, 0.75f, 0.15f);
        IconBorder->SetBrush(MakeSolidBrush(C));
    }
    if (NameLabel)
        NameLabel->SetText(Equipped != EItemId::None
            ? FItemRegistry::Get(Equipped).DisplayName
            : FText::FromString(TEXT("─")));
}

void UEquipmentSlotWidget::HandleClicked()
{
    if (OnClicked) OnClicked(SlotId);
}
