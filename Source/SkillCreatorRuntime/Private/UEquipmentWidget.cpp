#include "UEquipmentWidget.h"
#include "UEquipmentComponent.h"
#include "ItemRegistry.h"
#include "ItemData.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/BorderSlot.h"

FLinearColor UEquipmentWidget::GetItemColor(EItemId Id) const
{
    if (Id == EItemId::None) return FLinearColor(0.08f, 0.08f, 0.10f);
    switch (Id)
    {
        case EItemId::EquipBasicSword:   return FLinearColor(0.80f, 0.80f, 0.95f);
        case EItemId::EquipLeatherArmor: return FLinearColor(0.60f, 0.40f, 0.20f);
        case EItemId::EquipAmulet:       return FLinearColor(0.90f, 0.30f, 0.85f);
        default:                          return FLinearColor(0.85f, 0.75f, 0.15f);
    }
}

void UEquipmentWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    constexpr float PadX   = 8.f,  PadY   = 8.f;
    constexpr float IconS  = 38.f;
    constexpr float GapY   = 6.f;
    constexpr float TypeW  = 36.f, NameW  = 90.f, MidGap = 4.f;
    constexpr float TitleH = 20.f;
    float RowH   = IconS + GapY;
    float PanelW = PadX + TypeW + MidGap + IconS + MidGap + NameW + PadX;
    float PanelH = PadY + TitleH + 3.f * RowH + PadY;

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // 面板背景（UBorder 提供底色；其唯一子節點 Content 負責版面定位）
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EquipPanel"));
    Panel->SetBrushColor(FLinearColor(0.05f, 0.08f, 0.10f, 0.92f));
    Panel->SetPadding(FMargin(0.f));
    Root->AddChild(Panel);
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Panel->Slot))
    {
        S->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
        S->SetPosition(FVector2D(-(PanelW + 8.f), 20.f));
        S->SetSize(FVector2D(PanelW, PanelH));
        S->SetAlignment(FVector2D::ZeroVector);
    }

    // UBorder 只能有一個子節點；內層 UCanvasPanel 承載所有可定位子元素
    UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EquipContent"));
    Panel->AddChild(Content);

    // 標題
    UTextBlock* TitleLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TitleLbl->SetText(FText::FromString(TEXT("裝備欄  [X 關閉]")));
    {
        FSlateFontInfo F = TitleLbl->GetFont();
        F.Size = 12;
        TitleLbl->SetFont(F);
    }
    TitleLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.90f, 0.75f)));
    Content->AddChild(TitleLbl);
    if (UCanvasPanelSlot* TS = Cast<UCanvasPanelSlot>(TitleLbl->Slot))
        TS->SetOffsets(FMargin(PadX, PadY, PanelW - PadX * 2.f, TitleH));

    static const TCHAR* TypeNames[] = { TEXT("武器"), TEXT("防具"), TEXT("飾品") };
    for (int32 i = 0; i < 3; ++i)
    {
        float RowY = PadY + TitleH + i * RowH;

        // 類型標籤
        UTextBlock* TypeLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        TypeLbl->SetText(FText::FromString(TypeNames[i]));
        {
            FSlateFontInfo F = TypeLbl->GetFont();
            F.Size = 11;
            TypeLbl->SetFont(F);
        }
        TypeLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.60f, 0.70f, 0.65f)));
        Content->AddChild(TypeLbl);
        if (UCanvasPanelSlot* LS = Cast<UCanvasPanelSlot>(TypeLbl->Slot))
            LS->SetOffsets(FMargin(PadX, RowY + 11.f, TypeW, 16.f));

        // 裝備圖示槽（可點擊以卸下）
        UButton* SlotBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        if      (i == 0) SlotBtn->OnClicked.AddDynamic(this, &UEquipmentWidget::OnSlot0Clicked);
        else if (i == 1) SlotBtn->OnClicked.AddDynamic(this, &UEquipmentWidget::OnSlot1Clicked);
        else             SlotBtn->OnClicked.AddDynamic(this, &UEquipmentWidget::OnSlot2Clicked);
        Content->AddChild(SlotBtn);
        if (UCanvasPanelSlot* BS = Cast<UCanvasPanelSlot>(SlotBtn->Slot))
            BS->SetOffsets(FMargin(PadX + TypeW + MidGap, RowY, IconS, IconS));

        UBorder* SlotBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        SlotBg->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.12f));
        SlotBg->SetPadding(FMargin(0.f));
        SlotBtn->AddChild(SlotBg);
        SlotBorders[i] = SlotBg;

        // Icon 是 SlotBg（UBorder）的唯一子節點，透過 UBorderSlot 設定內縮
        UBorder* Icon = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Icon->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.10f));
        Icon->SetPadding(FMargin(0.f));
        SlotBg->AddChild(Icon);
        if (UBorderSlot* IS = Cast<UBorderSlot>(Icon->Slot))
        {
            IS->SetPadding(FMargin(4.f));
            IS->SetHorizontalAlignment(HAlign_Fill);
            IS->SetVerticalAlignment(VAlign_Fill);
        }
        IconBorders[i] = Icon;

        // 裝備名稱標籤
        UTextBlock* NameLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        NameLbl->SetText(FText::FromString(TEXT("─")));
        {
            FSlateFontInfo F = NameLbl->GetFont();
            F.Size = 11;
            NameLbl->SetFont(F);
        }
        NameLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.78f, 0.82f)));
        Content->AddChild(NameLbl);
        if (UCanvasPanelSlot* NS = Cast<UCanvasPanelSlot>(NameLbl->Slot))
            NS->SetOffsets(FMargin(PadX + TypeW + MidGap + IconS + MidGap, RowY + 10.f, NameW, 20.f));
        NameLabels[i] = NameLbl;
    }

    // Tooltip
    TooltipPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Tooltip"));
    TooltipPanel->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.12f, 0.95f));
    TooltipPanel->SetPadding(FMargin(8.f, 4.f));
    TooltipPanel->SetVisibility(ESlateVisibility::Hidden);
    Root->AddChild(TooltipPanel);

    TooltipText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    {
        FSlateFontInfo F = TooltipText->GetFont();
        F.Size = 12;
        TooltipText->SetFont(F);
    }
    TooltipText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.92f, 0.75f)));
    TooltipPanel->AddChild(TooltipText);
}

void UEquipmentWidget::Refresh(const UEquipmentComponent* Equip)
{
    if (!Equip) return;

    EItemId Equipped[3] = {
        Equip->WeaponId, Equip->ArmorId, Equip->AccessoryId
    };
    for (int32 i = 0; i < 3; ++i)
    {
        if (IconBorders[i])
            IconBorders[i]->SetBrushColor(GetItemColor(Equipped[i]));

        if (NameLabels[i])
        {
            FString Name = (Equipped[i] != EItemId::None)
                ? FItemRegistry::Get(Equipped[i]).DisplayName.ToString()
                : TEXT("─");
            NameLabels[i]->SetText(FText::FromString(Name));
        }
    }
}

void UEquipmentWidget::OnSlot0Clicked()
{
    OnUnequipRequested.ExecuteIfBound(EEquipmentSlotType::Weapon);
}
void UEquipmentWidget::OnSlot1Clicked()
{
    OnUnequipRequested.ExecuteIfBound(EEquipmentSlotType::Armor);
}
void UEquipmentWidget::OnSlot2Clicked()
{
    OnUnequipRequested.ExecuteIfBound(EEquipmentSlotType::Accessory);
}
