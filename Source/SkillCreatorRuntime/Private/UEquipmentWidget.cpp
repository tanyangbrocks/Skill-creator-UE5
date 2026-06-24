#include "UEquipmentWidget.h"
#include "UEquipmentComponent.h"
#include "UEquipmentSlotWidget.h"
#include "EquipmentSlot.h"
#include "ItemRegistry.h"
#include "ItemData.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "SlateBrushHelpers.h"

void UEquipmentWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    const TArray<FEquipmentSlotDef>& Defs = FEquipmentSlotRegistry::GetAll();

    constexpr float PadX = 8.f, PadY = 8.f, RowH = 44.f, TitleH = 20.f;
    constexpr float PanelW = 8.f + 36.f + 4.f + 38.f + 4.f + 90.f + 8.f; // 對齊 UEquipmentSlotWidget 內部欄寬
    const float PanelH = PadY + TitleH + Defs.Num() * RowH + PadY;

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EquipPanel"));
    Panel->SetBrush(MakeSolidBrush(FLinearColor(0.05f, 0.08f, 0.10f, 0.92f)));
    Panel->SetPadding(FMargin(0.f));
    Root->AddChild(Panel);
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Panel->Slot))
    {
        S->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
        S->SetPosition(FVector2D(-(PanelW + 8.f), 20.f));
        S->SetSize(FVector2D(PanelW, PanelH));
        S->SetAlignment(FVector2D::ZeroVector);
    }

    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EquipContent"));
    Panel->AddChild(Content);

    UTextBlock* TitleLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TitleLbl->SetText(FText::FromString(TEXT("裝備欄  [X 關閉]")));
    {
        FSlateFontInfo F = TitleLbl->GetFont(); F.Size = 12; TitleLbl->SetFont(F);
    }
    TitleLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.90f, 0.75f)));
    if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(TitleLbl))
        S->SetPadding(FMargin(PadX, PadY, PadX, 4.f));

    SlotList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SlotList"));
    Content->AddChildToVerticalBox(SlotList);

    SlotWidgets.Reset();
    for (const FEquipmentSlotDef& Def : Defs)
    {
        UEquipmentSlotWidget* SlotW = CreateWidget<UEquipmentSlotWidget>(this);
        SlotW->Setup(Def.Id, Def.DisplayName, [this](FName SlotId)
        {
            OnUnequipRequested.ExecuteIfBound(SlotId);
        });
        if (UVerticalBoxSlot* S = SlotList->AddChildToVerticalBox(SlotW))
            S->SetPadding(FMargin(PadX, 0.f, PadX, 0.f));
        SlotWidgets.Add(SlotW);
    }

    // Tooltip（目前僅保留容器，內容由 Refresh 之外的 hover 邏輯後續填，跟原版行為一致先佔位）
    TooltipPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Tooltip"));
    TooltipPanel->SetBrush(MakeSolidBrush(FLinearColor(0.05f, 0.05f, 0.12f, 0.95f)));
    TooltipPanel->SetPadding(FMargin(8.f, 4.f));
    TooltipPanel->SetVisibility(ESlateVisibility::Hidden);
    Root->AddChild(TooltipPanel);

    TooltipText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    {
        FSlateFontInfo F = TooltipText->GetFont(); F.Size = 12; TooltipText->SetFont(F);
    }
    TooltipText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.92f, 0.75f)));
    TooltipPanel->AddChild(TooltipText);
}

void UEquipmentWidget::Refresh(const UEquipmentComponent* Equip)
{
    if (!Equip) return;
    const TArray<FEquipmentSlotDef>& Defs = FEquipmentSlotRegistry::GetAll();
    for (int32 i = 0; i < Defs.Num() && i < SlotWidgets.Num(); ++i)
        if (SlotWidgets[i])
            SlotWidgets[i]->Refresh(Equip->GetEquipped(Defs[i].Id));
}
