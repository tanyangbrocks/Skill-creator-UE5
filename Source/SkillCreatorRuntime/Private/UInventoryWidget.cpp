#include "UInventoryWidget.h"
#include "UInventoryComponent.h"
#include "ItemRegistry.h"
#include "ItemData.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "SlateBrushHelpers.h"
#include "Engine/Texture2D.h"
#include "UObject/Class.h"

// ── 圖示貼圖快取（慣例路徑 /Game/Icons/ICO_{EnumName}）──────────────────────
// 首次呼叫觸發 LoadObject；後續直接回傳快取（包含 nullptr=確認不存在），不再呼叫 LoadObject

UTexture2D* UInventoryWidget::GetOrLoadIcon(EItemId Id)
{
    if (Id == EItemId::None) return nullptr;

    if (TObjectPtr<UTexture2D>* Hit = IconCache.Find(Id))
        return Hit->Get();  // 快取命中（nullptr 代表上次查過也沒有）

    UTexture2D* Tex = nullptr;
    if (const UEnum* E = StaticEnum<EItemId>())
    {
        const FString Name = E->GetNameStringByValue((int64)Id);
        const FString Path = FString::Printf(TEXT("/Game/Icons/ICO_%s.ICO_%s"), *Name, *Name);
        Tex = LoadObject<UTexture2D>(nullptr, *Path);
    }
    IconCache.Add(Id, Tex);  // 存入快取（nullptr 亦快取，避免反覆查詢）
    return Tex;
}

// ── 顏色查詢（fallback：圖示不存在時使用）──────────────────────────────────

FLinearColor UInventoryWidget::GetItemColor(EItemId Id) const
{
    if (Id == EItemId::None) return FLinearColor(0.10f, 0.10f, 0.12f);
    const FItemData& D = FItemRegistry::Get(Id);
    if (D.bIsPlaceable)
    {
        // 放置物件用材質顏色（暫用固定淡色；完整版從 MaterialRegistry 取）
        return FLinearColor(0.45f, 0.55f, 0.40f);
    }
    switch (Id)
    {
        case EItemId::ToolBasicPick:     return FLinearColor(0.75f, 0.75f, 0.82f);
        case EItemId::ToolIronPick:      return FLinearColor(0.60f, 0.62f, 0.68f);
        case EItemId::EquipBasicSword:   return FLinearColor(0.80f, 0.80f, 0.95f);
        case EItemId::EquipLeatherArmor: return FLinearColor(0.60f, 0.40f, 0.20f);
        case EItemId::EquipAmulet:       return FLinearColor(0.90f, 0.30f, 0.85f);
        default:                          return FLinearColor(0.85f, 0.75f, 0.15f);
    }
}

// ── NativeOnInitialized ─────────────────────────────────────────────────────

void UInventoryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    constexpr float SlotW = 38.f, SlotH = 38.f, GapX = 3.f, GapY = 3.f;
    constexpr float PadX  = 8.f,  PadY  = 8.f;

    float InnerW = Cols * SlotW + (Cols - 1) * GapX;
    float PanelW = InnerW + 2.f * PadX;

    // 行 Y 起點（相對於面板左上角）
    float Row0Y   = PadY + 20.f + 14.f;
    float Row1Y   = Row0Y + SlotH + GapY;
    float BagLblY = Row1Y + SlotH + GapY + 2.f;
    float Row2Y   = BagLblY + 14.f;

    float RowY[6];
    RowY[0] = Row0Y;
    RowY[1] = Row1Y;
    RowY[2] = Row2Y;
    for (int32 r = 3; r < 6; ++r) RowY[r] = Row2Y + (r - 2) * (SlotH + GapY);

    float PanelH = RowY[5] + SlotH + PadY;

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // 面板背景（UBorder 提供底色；其唯一子節點 Content 負責版面定位）
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InvPanel"));
    Panel->SetBrush(MakeSolidBrush(FLinearColor(0.05f, 0.05f, 0.10f, 0.92f)));
    Panel->SetPadding(FMargin(0.f));
    Root->AddChild(Panel);
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Panel->Slot))
    {
        S->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f));
        S->SetPosition(FVector2D(-PanelW - 5.f, 20.f));
        S->SetSize(FVector2D(PanelW, PanelH));
        S->SetAlignment(FVector2D::ZeroVector);
    }

    // UBorder 只能有一個子節點；內層 UCanvasPanel 承載所有可定位子元素
    UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InvContent"));
    Panel->AddChild(Content);

    // 標題
    UTextBlock* TitleLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TitleLbl->SetText(FText::FromString(TEXT("物品欄  [I 關閉]")));
    {
        FSlateFontInfo F = TitleLbl->GetFont();
        F.Size = 12;
        TitleLbl->SetFont(F);
    }
    TitleLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.80f, 0.95f)));
    Content->AddChild(TitleLbl);
    if (UCanvasPanelSlot* TS = Cast<UCanvasPanelSlot>(TitleLbl->Slot))
    {
        TS->SetPosition(FVector2D(PadX, PadY));
        TS->SetSize(FVector2D(InnerW, 16.f));
    }

    // 熱鍵欄標籤
    UTextBlock* HotLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    HotLbl->SetText(FText::FromString(TEXT("─ 熱鍵欄 ─")));
    {
        FSlateFontInfo F = HotLbl->GetFont();
        F.Size = 10;
        HotLbl->SetFont(F);
    }
    HotLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.60f, 0.75f, 0.60f)));
    Content->AddChild(HotLbl);
    if (UCanvasPanelSlot* LS = Cast<UCanvasPanelSlot>(HotLbl->Slot))
        LS->SetPosition(FVector2D(PadX, PadY + 20.f));

    // 背包標籤
    UTextBlock* BagLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    BagLbl->SetText(FText::FromString(TEXT("─ 背包 ─")));
    {
        FSlateFontInfo F = BagLbl->GetFont();
        F.Size = 10;
        BagLbl->SetFont(F);
    }
    BagLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.60f, 0.60f, 0.75f)));
    Content->AddChild(BagLbl);
    if (UCanvasPanelSlot* LS = Cast<UCanvasPanelSlot>(BagLbl->Slot))
        LS->SetPosition(FVector2D(PadX, BagLblY));

    // 30 個格子
    SlotBorders.Reserve(TotalSlots);
    IconBorders.Reserve(TotalSlots);
    CountLabels.Reserve(TotalSlots);

    CachedSlots.SetNum(TotalSlots);

    for (int32 i = 0; i < TotalSlots; ++i)
    {
        int32 Row = i / Cols;
        int32 Col = i % Cols;

        float X = PadX + Col * (SlotW + GapX);
        float Y = RowY[Row];

        // 格子背景（UBorder 提供底色，加入 Content 取得正確的 UCanvasPanelSlot）
        UBorder* SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        SlotBorder->SetBrush(MakeSolidBrush(FLinearColor(0.10f, 0.10f, 0.15f)));
        SlotBorder->SetPadding(FMargin(0.f));
        Content->AddChild(SlotBorder);
        if (UCanvasPanelSlot* SS = Cast<UCanvasPanelSlot>(SlotBorder->Slot))
            SS->SetOffsets(FMargin(X, Y, SlotW, SlotH));

        // UBorder 只能有一個子節點；UOverlay 承載圖示 + 計數標籤兩層
        UOverlay* SlotOvl = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        SlotBorder->AddChild(SlotOvl);

        // 圖示（四周 5px 內縮填滿）
        UBorder* Icon = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Icon->SetBrush(MakeSolidBrush(FLinearColor(0.10f, 0.10f, 0.12f)));
        Icon->SetPadding(FMargin(0.f));
        SlotOvl->AddChild(Icon);
        if (UOverlaySlot* IS = Cast<UOverlaySlot>(Icon->Slot))
        {
            IS->SetPadding(FMargin(5.f));
            IS->SetHorizontalAlignment(HAlign_Fill);
            IS->SetVerticalAlignment(VAlign_Fill);
        }

        // 計數標籤（對齊右下角）
        UTextBlock* Cnt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        {
            FSlateFontInfo F = Cnt->GetFont();
            F.Size = 9;
            Cnt->SetFont(F);
        }
        Cnt->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 0.85f)));
        Cnt->SetJustification(ETextJustify::Right);
        SlotOvl->AddChild(Cnt);
        if (UOverlaySlot* CS = Cast<UOverlaySlot>(Cnt->Slot))
        {
            CS->SetPadding(FMargin(0.f, 0.f, 2.f, 1.f));
            CS->SetHorizontalAlignment(HAlign_Right);
            CS->SetVerticalAlignment(VAlign_Bottom);
        }

        SlotBorders.Add(SlotBorder);
        IconBorders.Add(Icon);
        CountLabels.Add(Cnt);
    }

    // 拖曳浮動圖示
    DragFloatIcon = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DragIcon"));
    DragFloatIcon->SetBrush(MakeSolidBrush(FLinearColor(0.85f, 0.75f, 0.15f, 0.7f)));
    DragFloatIcon->SetPadding(FMargin(0.f));
    DragFloatIcon->SetVisibility(ESlateVisibility::Hidden);
    Root->AddChild(DragFloatIcon);
    if (UCanvasPanelSlot* DS = Cast<UCanvasPanelSlot>(DragFloatIcon->Slot))
        DS->SetSize(FVector2D(30.f, 30.f));

    // Tooltip
    TooltipPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Tooltip"));
    TooltipPanel->SetBrush(MakeSolidBrush(FLinearColor(0.05f, 0.05f, 0.12f, 0.95f)));
    TooltipPanel->SetPadding(FMargin(8.f, 4.f));
    TooltipPanel->SetVisibility(ESlateVisibility::Hidden);
    TooltipPanel->SetRenderTransformPivot(FVector2D::ZeroVector);
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

// ── 每幀更新（拖曳圖示跟隨滑鼠）───────────────────────────────────────────

void UInventoryWidget::NativeTick(const FGeometry& Geo, float Delta)
{
    Super::NativeTick(Geo, Delta);

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    float MX, MY;
    PC->GetMousePosition(MX, MY);
    FVector2D MousePos(MX, MY);

    // 拖曳圖示跟隨滑鼠（Godot Main.cs:939-943）
    if (DragSrcSlot >= 0 && DragFloatIcon)
    {
        if (UCanvasPanelSlot* DS = Cast<UCanvasPanelSlot>(DragFloatIcon->Slot))
            DS->SetPosition(MousePos - FVector2D(15.f, 15.f));
    }

    // Hover tooltip（Godot Main.cs:2589-2594：slot.MouseEntered → ShowFloatTooltip(DisplayName)）
    int32 Hovered = GetSlotUnderMouse();
    if (Hovered >= 0 && CachedSlots.IsValidIndex(Hovered) && !CachedSlots[Hovered].IsEmpty())
        ShowFloatTooltip(FItemRegistry::Get(CachedSlots[Hovered].ItemId).DisplayName.ToString(), MousePos);
    else
        HideFloatTooltip();
}

// ── 滑鼠事件（拖曳起點 + 終點）────────────────────────────────────────────

FReply UInventoryWidget::NativeOnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Ev)
{
    if (Ev.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        int32 Src = GetSlotUnderMouse();
        if (Src >= 0)
        {
            DragSrcSlot = Src;
            if (DragFloatIcon)
            {
                const FItemStack& Stack = CachedSlots[Src];
                if (!Stack.IsEmpty())
                {
                    UTexture2D* Icon = GetOrLoadIcon(Stack.ItemId);
                    if (Icon)
                    {
                        FSlateBrush B;
                        B.SetResourceObject(Icon);
                        B.DrawAs = ESlateBrushDrawType::Image;
                        B.Tiling = ESlateBrushTileType::NoTile;
                        DragFloatIcon->SetBrush(B);
                    }
                    else
                    {
                        DragFloatIcon->SetBrush(MakeSolidBrush(GetItemColor(Stack.ItemId)));
                    }
                    DragFloatIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
                }
            }
            return FReply::Handled();
        }
    }
    return Super::NativeOnMouseButtonDown(Geo, Ev);
}

FReply UInventoryWidget::NativeOnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& Ev)
{
    if (Ev.GetEffectingButton() == EKeys::LeftMouseButton && DragSrcSlot >= 0)
    {
        if (DragFloatIcon) DragFloatIcon->SetVisibility(ESlateVisibility::Hidden);

        int32 Dst = GetSlotUnderMouse();
        if (Dst >= 0 && Dst != DragSrcSlot)
            OnSlotSwapRequested.ExecuteIfBound(DragSrcSlot, Dst);
        else if (Dst == DragSrcSlot && !CachedSlots[DragSrcSlot].IsEmpty())
            OnSlotEquipRequested.ExecuteIfBound(DragSrcSlot);
        else if (Dst < 0 && PairedWidget.IsValid())
        {
            // 2026-06-23：跨欄拖曳——自己範圍內沒命中，改查 PairedWidget（UChestWidget 用）
            const int32 PairedDst = PairedWidget->GetSlotUnderMouse();
            if (PairedDst >= 0)
                OnCrossSwapRequested.ExecuteIfBound(DragSrcSlot, PairedDst);
        }

        DragSrcSlot = -1;
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonUp(Geo, Ev);
}

// ── 資料刷新 ────────────────────────────────────────────────────────────────

void UInventoryWidget::Refresh(const UInventoryComponent* Inv)
{
    if (!Inv) return;
    CachedActiveIdx = Inv->ActiveHotbarIndex;
    for (int32 i = 0; i < TotalSlots && i < Inv->Slots.Num(); ++i)
    {
        CachedSlots[i] = Inv->Slots[i];
        RefreshSlot(i);
    }
}

void UInventoryWidget::RefreshSlot(int32 Idx)
{
    if (!SlotBorders.IsValidIndex(Idx)) return;

    bool bActive = (Idx < HotbarSlots && Idx == CachedActiveIdx);
    FLinearColor BorderCol = bActive
        ? FLinearColor(0.95f, 0.80f, 0.20f)
        : FLinearColor(0.30f, 0.30f, 0.40f);
    SlotBorders[Idx]->SetBrush(MakeSolidBrush(BorderCol));

    const FItemStack& Stack = CachedSlots[Idx];
    if (Stack.IsEmpty())
    {
        IconBorders[Idx]->SetBrush(MakeSolidBrush(FLinearColor(0.10f, 0.10f, 0.12f)));
        CountLabels[Idx]->SetText(FText::GetEmpty());
    }
    else
    {
        UTexture2D* Icon = GetOrLoadIcon(Stack.ItemId);
        if (Icon)
        {
            FSlateBrush B;
            B.SetResourceObject(Icon);
            B.DrawAs = ESlateBrushDrawType::Image;
            B.Tiling = ESlateBrushTileType::NoTile;
            IconBorders[Idx]->SetBrush(B);
        }
        else
        {
            IconBorders[Idx]->SetBrush(MakeSolidBrush(GetItemColor(Stack.ItemId)));
        }
        CountLabels[Idx]->SetText(Stack.Count > 1
            ? FText::FromString(FString::Printf(TEXT("×%d"), Stack.Count))
            : FText::GetEmpty());
    }
}

int32 UInventoryWidget::GetSlotUnderMouse() const
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC) return -1;
    float MX, MY;
    PC->GetMousePosition(MX, MY);
    FVector2D MousePos(MX, MY);

    for (int32 i = 0; i < SlotBorders.Num(); ++i)
    {
        if (!SlotBorders[i]) continue;
        FGeometry Geo = SlotBorders[i]->GetCachedGeometry();
        FVector2D Local = Geo.AbsoluteToLocal(MousePos);
        FVector2D Size  = Geo.GetLocalSize();
        if (Local.X >= 0 && Local.Y >= 0 && Local.X < Size.X && Local.Y < Size.Y)
            return i;
    }
    return -1;
}

void UInventoryWidget::ShowFloatTooltip(const FString& Text, FVector2D ScreenPos)
{
    if (!TooltipPanel || !TooltipText) return;
    TooltipText->SetText(FText::FromString(Text));
    TooltipPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (UCanvasPanelSlot* TS = Cast<UCanvasPanelSlot>(TooltipPanel->Slot))
        TS->SetPosition(ScreenPos + FVector2D(14.f, -20.f));
}

void UInventoryWidget::HideFloatTooltip()
{
    if (TooltipPanel) TooltipPanel->SetVisibility(ESlateVisibility::Hidden);
}
