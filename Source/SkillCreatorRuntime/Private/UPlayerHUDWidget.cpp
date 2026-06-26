#include "UPlayerHUDWidget.h"
#include "UHpMpCircleWidget.h"
#include "UAbnormalStatusBarWidget.h"
#include "UGameClockSubsystem.h"
#include "UInventoryComponent.h"
#include "UCharacterStateComponent.h"
#include "ItemStack.h"
#include "ItemRegistry.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Styling/SlateTypes.h"
#include "SlateBrushHelpers.h"

// ── Static layout helper ──────────────────────────────────────────────────

void UPlayerHUDWidget::Pin(UWidget* W, FVector2D Pos, FVector2D Size,
                            FAnchors Anchors, FVector2D Align)
{
    if (!W) return;
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(W->Slot))
    {
        S->SetAnchors(Anchors);
        S->SetPosition(Pos);
        S->SetSize(Size);
        S->SetAlignment(Align);
    }
}

// Anchor bottom-left shorthand (all survival / level elements)
static void PinBL(UWidget* W, FVector2D Pos, FVector2D Size)
{
    if (!W) return;
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(W->Slot))
    {
        S->SetAnchors(FAnchors(0.f, 1.f, 0.f, 1.f));
        S->SetPosition(Pos);
        S->SetSize(Size);
        S->SetAlignment(FVector2D::ZeroVector);
    }
}

// ── Colour tables (match Godot Main.cs) ───────────────────────────────────

FLinearColor UPlayerHUDWidget::ManaColor(FName Key)
{
    static const TMap<FName, FLinearColor> Map = {
        { "wu_dao",     FLinearColor(0.95f, 0.50f, 0.15f) },
        { "xian_dao",   FLinearColor(0.95f, 0.88f, 0.25f) },
        { "fa_dao",     FLinearColor(0.65f, 0.30f, 0.95f) },
        { "yi_dao",     FLinearColor(0.28f, 0.85f, 0.40f) },
        { "hun_dao",    FLinearColor(0.25f, 0.75f, 0.85f) },
        { "gui_dao",    FLinearColor(0.35f, 0.50f, 0.90f) },
        { "mo_fa",      FLinearColor(0.70f, 0.18f, 0.80f) },
        { "yao_li",     FLinearColor(0.70f, 0.82f, 0.18f) },
        { "ao_shu",     FLinearColor(0.15f, 0.85f, 0.90f) },
        { "shen_sheng", FLinearColor(0.95f, 0.92f, 0.50f) },
        { "yuan_neng",  FLinearColor(0.90f, 0.55f, 0.18f) },
        { "xing_neng",  FLinearColor(0.58f, 0.75f, 1.00f) },
        { "ji_neng",    FLinearColor(0.48f, 0.65f, 0.48f) },
        { "zhi_ye",     FLinearColor(0.55f, 0.42f, 0.88f) },
        { "chao_neng",  FLinearColor(0.22f, 0.95f, 0.88f) },
        { "shen_li",    FLinearColor(0.95f, 0.95f, 0.78f) },
        { "gai_nian",   FLinearColor(0.75f, 0.52f, 0.92f) },
        { "xun_neng",   FLinearColor(0.28f, 0.85f, 0.68f) },
    };
    const FLinearColor* Found = Map.Find(Key);
    return Found ? *Found : FLinearColor(0.58f, 0.58f, 0.62f);
}

FString UPlayerHUDWidget::ManaAbbrev(FName Key)
{
    static const TMap<FName, FString> Map = {
        { "wu_dao",     TEXT("武") }, { "xian_dao",   TEXT("仙") },
        { "fa_dao",     TEXT("法") }, { "yi_dao",     TEXT("醫") },
        { "hun_dao",    TEXT("魂") }, { "gui_dao",    TEXT("鬼") },
        { "mo_fa",      TEXT("魔") }, { "yao_li",     TEXT("妖") },
        { "ao_shu",     TEXT("奧") }, { "shen_sheng", TEXT("聖") },
        { "yuan_neng",  TEXT("源") }, { "xing_neng",  TEXT("星") },
        { "ji_neng",    TEXT("機") }, { "zhi_ye",     TEXT("智") },
        { "chao_neng",  TEXT("超") }, { "shen_li",    TEXT("神") },
        { "gai_nian",   TEXT("念") }, { "xun_neng",   TEXT("循") },
    };
    const FString* Found = Map.Find(Key);
    return Found ? *Found : Key.ToString().Left(1);
}

FLinearColor UPlayerHUDWidget::ItemIconColor(EItemId Id)
{
    if (Id == EItemId::None) return FLinearColor(0.18f, 0.18f, 0.22f);
    const FItemData& D = FItemRegistry::Get(Id);
    if (D.bIsPlaceable) return FLinearColor(0.45f, 0.55f, 0.40f);
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

// ══════════════════════════════════════════════════════════════════════════
// NativeOnInitialized
// ══════════════════════════════════════════════════════════════════════════

void UPlayerHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // 2026-06-23 診斷：使用者多次回報準心/熱鍵欄/生存條從未出現，外觀疑似舊版 WBP_PlayerHUD_C
    // 的進度條（預設藍色、無自訂顏色）。GameMode/HUDClass/HUDWidgetClass 鏈路靜態讀過幾輪都
    // 顯示應該是純 C++ 路徑，加 log 直接確認 HpBar 是否在這裡就已經非 null（代表真的繼承到
    // Blueprint 綁定），而不是再猜。
    UE_LOG(LogTemp, Warning, TEXT("UPlayerHUDWidget::NativeOnInitialized HpBar already=%s class=%s"),
        HpBar ? TEXT("non-null") : TEXT("null"), *GetClass()->GetName());

    // 如果 Blueprint 子類已綁定 widget，僅調整位置後退出
    if (HpBar)
    {
        Pin(HpBar,        { 20, 20 }, { 180, 16 });
        Pin(HpTextBlock,  { 208, 20 }, { 120, 16 });
        Pin(MpBar,        { 20, 44 }, { 180, 16 });
        Pin(MpTextBlock,  { 208, 44 }, { 120, 16 });
        Pin(StaminaBar,   { 20, 68 }, { 180, 12 });
        Pin(HotBarBox,    { 0, -60 }, { 400, 48 },
            FAnchors(0.5f, 1.f, 0.5f, 1.f), { 0.5f, 1.f });
        return;
    }

    // ── 純 C++ 建構 ─────────────────────────────────────────────────────
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // ① HP / MP / Stamina / Mood 條（左上角，小型）
    auto AddBar = [&](FName N, FLinearColor C, FVector2D Pos) -> UProgressBar*
    {
        UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(
            UProgressBar::StaticClass(), N);
        Bar->SetFillColorAndOpacity(C);
        Root->AddChild(Bar);
        Pin(Bar, Pos, { 180, 16 });
        return Bar;
    };
    auto AddTxt = [&](FName N, FVector2D Pos) -> UTextBlock*
    {
        UTextBlock* TB = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), N);
        TB->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Root->AddChild(TB);
        Pin(TB, Pos, { 120, 16 });
        return TB;
    };

    HpBar      = AddBar(TEXT("HpBar"),      FLinearColor(0.85f, 0.12f, 0.12f), { 20, 20 });
    MpBar      = AddBar(TEXT("MpBar"),      FLinearColor(0.15f, 0.45f, 0.90f), { 20, 44 });
    StaminaBar = AddBar(TEXT("StaminaBar"), FLinearColor(0.15f, 0.80f, 0.25f), { 20, 68 });
    HpTextBlock = AddTxt(TEXT("HpText"), { 208, 20 });
    MpTextBlock = AddTxt(TEXT("MpText"), { 208, 44 });
    // 舊版長條被 HP/MP 圓形水缸取代，隱藏避免重疊
    HpBar->SetVisibility(ESlateVisibility::Collapsed);
    MpBar->SetVisibility(ESlateVisibility::Collapsed);
    StaminaBar->SetVisibility(ESlateVisibility::Collapsed);
    HpTextBlock->SetVisibility(ESlateVisibility::Collapsed);
    MpTextBlock->SetVisibility(ESlateVisibility::Collapsed);

    // ① HP/MP 圓形水缸（左上角 10,10，120×120）
    BuildHpMpCircle(Root);

    // ② 準心
    BuildCrosshair(Root);

    // ③ HP 文字標籤（底部左側）
    HpLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HpLabel"));
    {
        FSlateFontInfo F = HpLabel->GetFont(); F.Size = 12; HpLabel->SetFont(F);
    }
    HpLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.30f, 0.30f)));
    Root->AddChild(HpLabel);
    PinBL(HpLabel, { 10.f, -92.f }, { 180.f, 16.f });

    // ④ 副手欄（熱鍵欄左側圓形槽）+ 物品熱鍵欄（底部 10 槽）+ 拿取槽（右側）
    BuildOffhandSlot(Root);
    BuildItemHotbar(Root);
    BuildCarrySlot(Root);

    // ⑤ 生存條
    BuildSurvivalBars(Root);

    // ⑥ 等級 / XP / 境界
    BuildLevelHud(Root);

    // ⑦ 動態法力條 VBox（底部左側，向上生長）
    BuildManaHud(Root);

    // ⑧ 法術熱鍵欄文字（底部中央）—— 2026-06-22 使用者要求不要顯示這個文字提示，
    // 直接 Collapsed。底層 UpdateSpellHotBar() 邏輯不變，繼續更新 HotBarBox 的子項，
    // 只是整個容器不可見，不影響施法（U/I/O/P 偵測在 HandleSpellInput() 是獨立的）。
    HotBarBox = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("HotBarBox"));
    Root->AddChild(HotBarBox);
    Pin(HotBarBox, { 0.f, -60.f }, { 400.f, 48.f },
        FAnchors(0.5f, 1.f, 0.5f, 1.f), { 0.5f, 1.f });
    HotBarBox->SetVisibility(ESlateVisibility::Collapsed);

    // ⑨ 形狀指示器
    ShapeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShapeLabel"));
    {
        FSlateFontInfo F = ShapeLabel->GetFont(); F.Size = 10; ShapeLabel->SetFont(F);
    }
    ShapeLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.60f)));
    Root->AddChild(ShapeLabel);
    PinBL(ShapeLabel, { 106.f, -155.f }, { 160.f, 14.f });
    SetActiveShape(EPlacementShape::Single, 1);

    // ⑩ 提示文字（底部中央）
    BuildHintLabel(Root);

    // ⑪ 死亡遮罩
    BuildDeathScreenOverlay(Root);

    // ⑫ 境界突破通知
    BuildBreakthroughLabel(Root);

    // ⑬ 浮動 Tooltip
    BuildFloatTooltip(Root);

    // ⑭ 遊戲時鐘（右上角）
    BuildClock(Root);

    // ⑮ 小地圖佔位圓形（右上角，時鐘左側）
    BuildMinimap(Root);

    // ⑯ 異常狀態圖示列（頂部中央）
    BuildAbnormalStatusBar(Root);
}

// ══════════════════════════════════════════════════════════════════════════
// BuildXxx helpers
// ══════════════════════════════════════════════════════════════════════════

void UPlayerHUDWidget::BuildCrosshair(UCanvasPanel* Root)
{
    // 十字準心：水平 20×2 + 垂直 2×20，帶黑色陰影
    struct CrossPart { FVector2D Pos; FVector2D Size; FLinearColor Color; };
    static const CrossPart Parts[] = {
        // 陰影（深色，+1px 偏移）
        { { -11.f,  0.f }, { 22.f, 3.f }, FLinearColor(0.f, 0.f, 0.f, 0.45f) },
        { {   0.f,-11.f }, {  3.f,22.f }, FLinearColor(0.f, 0.f, 0.f, 0.45f) },
        // 白色準心
        { { -10.f, -1.f }, { 20.f, 2.f }, FLinearColor(1.f, 1.f, 1.f, 0.75f) },
        { {  -1.f,-10.f }, {  2.f,20.f }, FLinearColor(1.f, 1.f, 1.f, 0.75f) },
    };
    for (const auto& P : Parts)
    {
        UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        B->SetBrush(MakeSolidBrush(P.Color));
        B->SetPadding(FMargin(0.f));
        Root->AddChild(B);
        Pin(B, P.Pos, P.Size,
            FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D::ZeroVector);
    }
}

void UPlayerHUDWidget::BuildOffhandSlot(UCanvasPanel* Root)
{
    // 副手欄：圓形 48×48，位於熱鍵欄最左格再往左 8px（StartX=10, 方格 48+gap4=52, 再-8 gap = 10-52-8=-50... 用負值）
    // 熱鍵欄已右移至 StartX=66，副手欄放在 10（與左側 HUD 齊）
    constexpr float SlotW = 48.f, SlotH = 48.f;
    constexpr float X = 10.f, Y = -132.f;
    constexpr float CornerR = SlotW * 0.5f;  // 全圓角 = 圓形

    OffhandBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    OffhandBorder->SetPadding(FMargin(0.f));
    Root->AddChild(OffhandBorder);
    PinBL(OffhandBorder, { X, Y }, { SlotW, SlotH });

    // 內層 CanvasPanel（承載 Icon / Count / Key 標籤）
    UCanvasPanel* SlotCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    OffhandBorder->AddChild(SlotCanvas);
    if (UBorderSlot* BS = Cast<UBorderSlot>(SlotCanvas->Slot))
    {
        BS->SetHorizontalAlignment(HAlign_Fill);
        BS->SetVerticalAlignment(VAlign_Fill);
    }

    // 物品色塊圖示
    OffhandIconBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    OffhandIconBorder->SetBrush(MakeSolidBrush(FLinearColor(0.18f, 0.18f, 0.22f)));
    OffhandIconBorder->SetPadding(FMargin(0.f));
    SlotCanvas->AddChild(OffhandIconBorder);
    if (UCanvasPanelSlot* IS = Cast<UCanvasPanelSlot>(OffhandIconBorder->Slot))
        IS->SetOffsets(FMargin(8.f, 8.f, 32.f, 24.f));

    // 數量標籤（右下）
    OffhandCountLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    {
        FSlateFontInfo F = OffhandCountLabel->GetFont(); F.Size = 9;
        OffhandCountLabel->SetFont(F);
    }
    OffhandCountLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 0.85f)));
    OffhandCountLabel->SetJustification(ETextJustify::Right);
    SlotCanvas->AddChild(OffhandCountLabel);
    if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(OffhandCountLabel->Slot))
        CS->SetOffsets(FMargin(28.f, 32.f, 18.f, 14.f));

    // ` 鍵提示（左上小字）
    OffhandKeyLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    {
        FSlateFontInfo F = OffhandKeyLabel->GetFont(); F.Size = 9;
        OffhandKeyLabel->SetFont(F);
    }
    OffhandKeyLabel->SetText(FText::FromString(TEXT("`")));
    OffhandKeyLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.50f, 0.50f, 0.60f)));
    SlotCanvas->AddChild(OffhandKeyLabel);
    if (UCanvasPanelSlot* KS = Cast<UCanvasPanelSlot>(OffhandKeyLabel->Slot))
        KS->SetOffsets(FMargin(3.f, 2.f, 14.f, 12.f));

    // 初始化外框外觀（非 active 狀態）
    UpdateOffhandSlot(FItemStack(), false);
}

void UPlayerHUDWidget::BuildItemHotbar(UCanvasPanel* Root)
{
    constexpr float SlotW = 48.f, SlotH = 48.f, Gap = 4.f;
    // 副手欄佔 10（X） + 48（寬）+ 8（間距）= 66，熱鍵欄從此開始
    constexpr float StartX = 66.f, StartY = -132.f;
    constexpr int32 Count  = 10;

    ItemSlotBorders.Reserve(Count);
    ItemIconBorders.Reserve(Count);
    ItemCountLabels.Reserve(Count);
    ItemKeyLabels.Reserve(Count);

    for (int32 i = 0; i < Count; ++i)
    {
        float X = StartX + i * (SlotW + Gap);

        // 外框
        // Bug H-3 修復：不在此設 SetBrushColor——BrushColor 預設 White，
        // 若同時呼叫 SetBrushColor(dark) 和 UpdateItemHotbar 的 SetBrush(TintColor=dark)，
        // Slate 渲染 BrushColor × TintColor ≈ 0.01（近黑，完全不可見）。
        // UpdateItemHotbar 每幀呼叫 SetBrush() 設定正確 TintColor，BrushColor 維持 White。
        UBorder* HotbarBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        HotbarBorder->SetPadding(FMargin(0.f));
        Root->AddChild(HotbarBorder);
        PinBL(HotbarBorder, { X, StartY }, { SlotW, SlotH });
        ItemSlotBorders.Add(HotbarBorder);

        // UBorder 只能一個子節點（UContentWidget），用內層 CanvasPanel 承載 Icon/Count/Key
        // （Bug H-1：原本直接 HotbarBorder->AddChild(X) 後的 Slot 型別是 UBorderSlot，
        //  Cast<UCanvasPanelSlot> 永遠 null，SetOffsets 從未執行）
        UCanvasPanel* SlotCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
        HotbarBorder->AddChild(SlotCanvas);
        if (UBorderSlot* BS = Cast<UBorderSlot>(SlotCanvas->Slot))
        {
            BS->SetHorizontalAlignment(HAlign_Fill);
            BS->SetVerticalAlignment(VAlign_Fill);
        }

        // 物品色塊圖示（左上）
        UBorder* Icon = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Icon->SetBrush(MakeSolidBrush(FLinearColor(0.18f, 0.18f, 0.22f)));
        Icon->SetPadding(FMargin(0.f));
        SlotCanvas->AddChild(Icon);
        if (UCanvasPanelSlot* IS = Cast<UCanvasPanelSlot>(Icon->Slot))
            IS->SetOffsets(FMargin(6.f, 6.f, 36.f, 28.f));
        ItemIconBorders.Add(Icon);

        // 數量標籤（右下）
        UTextBlock* Cnt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        {
            FSlateFontInfo F = Cnt->GetFont(); F.Size = 9; Cnt->SetFont(F);
        }
        Cnt->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 0.85f)));
        Cnt->SetJustification(ETextJustify::Right);
        SlotCanvas->AddChild(Cnt);
        if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(Cnt->Slot))
            CS->SetOffsets(FMargin(28.f, 34.f, 18.f, 12.f));
        ItemCountLabels.Add(Cnt);

        // 數字鍵提示（左上小字）
        UTextBlock* Key = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        {
            FSlateFontInfo F = Key->GetFont(); F.Size = 9; Key->SetFont(F);
        }
        Key->SetText(FText::FromString(i == 9 ? TEXT("0") : FString::Printf(TEXT("%d"), i + 1)));
        Key->SetColorAndOpacity(FSlateColor(FLinearColor(0.50f, 0.50f, 0.60f)));
        SlotCanvas->AddChild(Key);
        if (UCanvasPanelSlot* KS = Cast<UCanvasPanelSlot>(Key->Slot))
            KS->SetOffsets(FMargin(3.f, 2.f, 14.f, 12.f));
        ItemKeyLabels.Add(Key);
    }
}

void UPlayerHUDWidget::BuildSurvivalBars(UCanvasPanel* Root)
{
    // 5 條垂直生存條（底部左側）：飢餓 / 口渴 / 氧氣 / 體力 / 溫度
    // 垂直填充：由底往上（Ratio 增大 = 填充往上生長）
    // BarBottomAbove: 條底距螢幕底部的像素數（讓條底與熱鍵欄頂有 18px 間距）
    constexpr float BarW           = 10.f;
    constexpr float BarH           = 80.f;
    constexpr float GapX           = 18.f;  // BarW(10) + 間距(8)
    constexpr float StartX         = 10.f;
    constexpr float BarBottomAbove = 150.f;  // 條底距螢幕底部（熱鍵欄頂=132, 留 18px 空隙）
    constexpr float BarTopY        = -(BarBottomAbove + BarH);  // = -230（條頂的 PinBL Y）
    constexpr float LabelY         = -(BarBottomAbove - 4.f);   // = -146（標籤頂的 PinBL Y，條底下方 4px）
    constexpr float LabelH         = 14.f;
    constexpr float LabelW         = 20.f;

    static const struct { const TCHAR* Name; FLinearColor Fill; } Defs[] = {
        { TEXT("飢餓"), FLinearColor(0.60f, 0.88f, 0.22f) },
        { TEXT("口渴"), FLinearColor(0.28f, 0.78f, 1.00f) },
        { TEXT("氧氣"), FLinearColor(0.45f, 0.65f, 1.00f) },
        { TEXT("體力"), FLinearColor(1.00f, 0.60f, 0.22f) },
    };
    constexpr int32 NStd = UE_ARRAY_COUNT(Defs);
    SurvivalBarFills.Reserve(NStd);
    SurvivalValLabels.Reserve(NStd);

    for (int32 i = 0; i < NStd; ++i)
    {
        float X = StartX + i * GapX;

        // 背景（暗色）
        UBorder* BarBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        BarBg->SetBrush(MakeSolidBrush(FLinearColor(0.10f, 0.10f, 0.16f)));
        BarBg->SetPadding(FMargin(0.f));
        Root->AddChild(BarBg);
        PinBL(BarBg, { X, BarTopY }, { BarW, BarH });

        // 填充（直接加到 Root，避免 UBorderSlot Cast 失敗問題；初始滿格，UpdateSurvival 每幀調整）
        UBorder* Fill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Fill->SetBrush(MakeSolidBrush(Defs[i].Fill));
        Fill->SetPadding(FMargin(0.f));
        Root->AddChild(Fill);
        PinBL(Fill, { X, BarTopY }, { BarW, BarH });
        SurvivalBarFills.Add(Fill);

        // 名稱標籤（條正下方，危急時變紅）
        UTextBlock* NmLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        NmLbl->SetText(FText::FromString(Defs[i].Name));
        {
            FSlateFontInfo F = NmLbl->GetFont(); F.Size = 8; NmLbl->SetFont(F);
        }
        NmLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.60f)));
        NmLbl->SetJustification(ETextJustify::Center);
        Root->AddChild(NmLbl);
        PinBL(NmLbl, { X + BarW * 0.5f - LabelW * 0.5f, LabelY }, { LabelW, LabelH });
        SurvivalValLabels.Add(NmLbl);
    }

    // ── 體溫雙向條（第 5 格）──────────────────────────────────────────
    // 正常體溫時游標在中點；偏熱往上（橙紅填充，從中點向上）；偏冷往下（藍色填充，從中點向下）
    constexpr float TempBarCenterAbove = BarBottomAbove + BarH * 0.5f;  // 190px above screen
    float TempX = StartX + NStd * GapX;  // X = 10 + 4*18 = 82

    // 背景
    TempBarBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    TempBarBg->SetBrush(MakeSolidBrush(FLinearColor(0.08f, 0.08f, 0.12f)));
    TempBarBg->SetPadding(FMargin(0.f));
    Root->AddChild(TempBarBg);
    PinBL(TempBarBg, { TempX, BarTopY }, { BarW, BarH });

    // 中線標記（1px 白線，代表正常體溫位置）
    UBorder* CenterLine = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    CenterLine->SetBrush(MakeSolidBrush(FLinearColor(0.7f, 0.7f, 0.7f, 0.85f)));
    CenterLine->SetPadding(FMargin(0.f));
    Root->AddChild(CenterLine);
    PinBL(CenterLine, { TempX, -(TempBarCenterAbove + 0.5f) }, { BarW, 1.f });

    // 熱填充（從中線往上，橙紅；初始高度=0，UpdateSurvival 調整 Y 和 H）
    TempHotFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    TempHotFill->SetBrush(MakeSolidBrush(FLinearColor(0.95f, 0.40f, 0.15f)));
    TempHotFill->SetPadding(FMargin(0.f));
    Root->AddChild(TempHotFill);
    PinBL(TempHotFill, { TempX, -TempBarCenterAbove }, { BarW, 0.f });

    // 冷填充（從中線往下，藍色；初始高度=0，UpdateSurvival 調整 H）
    TempColdFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    TempColdFill->SetBrush(MakeSolidBrush(FLinearColor(0.30f, 0.55f, 1.00f)));
    TempColdFill->SetPadding(FMargin(0.f));
    Root->AddChild(TempColdFill);
    PinBL(TempColdFill, { TempX, -TempBarCenterAbove }, { BarW, 0.f });

    // 體溫標籤
    UTextBlock* TempNm = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TempNm->SetText(FText::FromString(TEXT("溫度")));
    {
        FSlateFontInfo F = TempNm->GetFont(); F.Size = 8; TempNm->SetFont(F);
    }
    TempNm->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.60f)));
    TempNm->SetJustification(ETextJustify::Center);
    Root->AddChild(TempNm);
    PinBL(TempNm, { TempX + BarW * 0.5f - LabelW * 0.5f, LabelY }, { LabelW, LabelH });
}

void UPlayerHUDWidget::BuildLevelHud(UCanvasPanel* Root)
{
    // 裝備標籤（左下角，生存條左上方）
    EquipLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EquipLabel"));
    {
        FSlateFontInfo F = EquipLabel->GetFont(); F.Size = 11; EquipLabel->SetFont(F);
    }
    EquipLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.85f, 0.65f)));
    Root->AddChild(EquipLabel);
    PinBL(EquipLabel, { 106.f, -248.f }, { 300.f, 14.f });

    // LV 標籤（底部中央，XP 條左上方）：anchor 底部中央
    LevelLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LvLabel"));
    {
        FSlateFontInfo F = LevelLabel->GetFont(); F.Size = 11; LevelLabel->SetFont(F);
    }
    LevelLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.25f)));
    Root->AddChild(LevelLabel);
    Pin(LevelLabel, { -256.f, -84.f }, { 80.f, 14.f },
        FAnchors(0.5f, 1.f, 0.5f, 1.f), FVector2D(0.f, 1.f));

    // 境界標籤（LV 右側）
    TierLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TierLabel"));
    {
        FSlateFontInfo F = TierLabel->GetFont(); F.Size = 11; TierLabel->SetFont(F);
    }
    TierLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)));
    Root->AddChild(TierLabel);
    Pin(TierLabel, { -174.f, -84.f }, { 120.f, 14.f },
        FAnchors(0.5f, 1.f, 0.5f, 1.f), FVector2D(0.f, 1.f));

    // XP 文字（XP 條右側外）
    XpLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("XpLabel"));
    {
        FSlateFontInfo F = XpLabel->GetFont(); F.Size = 9; XpLabel->SetFont(F);
    }
    XpLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.50f, 0.50f, 0.55f)));
    Root->AddChild(XpLabel);
    Pin(XpLabel, { 264.f, -84.f }, { 130.f, 14.f },
        FAnchors(0.5f, 1.f, 0.5f, 1.f), FVector2D(0.f, 1.f));

    // XP 條背景（底部水平置中，寬 520px，高 8px）
    // anchor(0.5, 1)、alignment(0.5, 1)：widget 底部中心對齊 offset 點
    UBorder* XpBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("XpBg"));
    XpBg->SetBrush(MakeSolidBrush(FLinearColor(0.12f, 0.12f, 0.18f)));
    XpBg->SetPadding(FMargin(0.f));
    Root->AddChild(XpBg);
    Pin(XpBg, { 0.f, -72.f }, { XpBarMaxWidth, 8.f },
        FAnchors(0.5f, 1.f, 0.5f, 1.f), FVector2D(0.5f, 1.f));

    // XP 填充（同位置，初始寬度 0）
    XpBarFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("XpFill"));
    XpBarFill->SetBrush(MakeSolidBrush(FLinearColor(0.25f, 0.65f, 0.95f)));
    XpBarFill->SetPadding(FMargin(0.f));
    Root->AddChild(XpBarFill);
    Pin(XpBarFill, { 0.f, -72.f }, { 0.f, 8.f },
        FAnchors(0.5f, 1.f, 0.5f, 1.f), FVector2D(0.5f, 1.f));
}

void UPlayerHUDWidget::BuildManaHud(UCanvasPanel* Root)
{
    ManaHudVBox = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("ManaHud"));
    Root->AddChild(ManaHudVBox);
    // 底部左側，由下往上生長 → anchor bottom-left，Align(0,1)=pivot 在 widget 底部
    // 若 Align=(0,0)（預設）：widget 頂邊在 y=720-30=690，往下延伸到 890，完全超出畫面。
    // Align=(0,1)：widget 底邊在 y=720-30=690，往上延伸到 490，正確在畫面內。
    Pin(ManaHudVBox, { 10.f, -30.f }, { 160.f, 200.f },
        FAnchors(0.f, 1.f, 0.f, 1.f), { 0.f, 1.f });
}

void UPlayerHUDWidget::BuildDeathScreenOverlay(UCanvasPanel* Root)
{
    DeathScreen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DeathScreen"));
    DeathScreen->SetBrush(MakeSolidBrush(FLinearColor(0.f, 0.f, 0.f, 0.72f)));
    DeathScreen->SetPadding(FMargin(0.f));
    DeathScreen->SetVisibility(ESlateVisibility::Hidden);
    Root->AddChild(DeathScreen);
    if (UCanvasPanelSlot* DS = Cast<UCanvasPanelSlot>(DeathScreen->Slot))
        DS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));

    // 中央 VBox
    UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    DeathScreen->AddChild(VBox);
    if (UCanvasPanelSlot* VS = Cast<UCanvasPanelSlot>(VBox->Slot))
    {
        VS->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
        VS->SetPosition(FVector2D(-130.f, -72.f));
        VS->SetSize(FVector2D(260.f, 144.f));
    }

    UTextBlock* DeathTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    DeathTitle->SetText(FText::FromString(TEXT("你已死亡")));
    {
        FSlateFontInfo F = DeathTitle->GetFont(); F.Size = 38; DeathTitle->SetFont(F);
    }
    DeathTitle->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.18f, 0.18f)));
    DeathTitle->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(DeathTitle))
    {
        VS->SetHorizontalAlignment(HAlign_Fill);
        VS->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
    }

    // D-3: 真正可點的重生按鈕（Godot Main.cs:1832-1837 對應行為）
    UButton* RespawnBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RespawnBtn"));
    RespawnBtn->OnClicked.AddDynamic(this, &UPlayerHUDWidget::OnRespawnButtonClicked);

    UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    BtnLabel->SetText(FText::FromString(TEXT("重生")));
    {
        FSlateFontInfo F = BtnLabel->GetFont(); F.Size = 18; BtnLabel->SetFont(F);
    }
    BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f)));
    BtnLabel->SetJustification(ETextJustify::Center);
    RespawnBtn->AddChild(BtnLabel);

    if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(RespawnBtn))
        VS->SetHorizontalAlignment(HAlign_Center);
}

void UPlayerHUDWidget::OnRespawnButtonClicked()
{
    OnRespawnRequested.ExecuteIfBound();
}

void UPlayerHUDWidget::BuildBreakthroughLabel(UCanvasPanel* Root)
{
    BreakthroughLabel = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("BreakLabel"));
    {
        FSlateFontInfo F = BreakthroughLabel->GetFont(); F.Size = 20; BreakthroughLabel->SetFont(F);
    }
    BreakthroughLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.25f)));
    BreakthroughLabel->SetJustification(ETextJustify::Center);
    BreakthroughLabel->SetVisibility(ESlateVisibility::Hidden);
    Root->AddChild(BreakthroughLabel);
    Pin(BreakthroughLabel, { -120.f, -60.f }, { 240.f, 40.f },
        FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
}

void UPlayerHUDWidget::BuildFloatTooltip(UCanvasPanel* Root)
{
    FloatTooltipPanel = WidgetTree->ConstructWidget<UBorder>(
        UBorder::StaticClass(), TEXT("FloatTip"));
    FloatTooltipPanel->SetBrush(MakeSolidBrush(FLinearColor(0.05f, 0.05f, 0.12f, 0.95f)));
    FloatTooltipPanel->SetPadding(FMargin(8.f, 4.f));
    FloatTooltipPanel->SetVisibility(ESlateVisibility::Hidden);
    Root->AddChild(FloatTooltipPanel);

    FloatTooltipText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    {
        FSlateFontInfo F = FloatTooltipText->GetFont(); F.Size = 12; FloatTooltipText->SetFont(F);
    }
    FloatTooltipText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.92f, 0.75f)));
    FloatTooltipPanel->AddChild(FloatTooltipText);
}

void UPlayerHUDWidget::BuildHintLabel(UCanvasPanel* Root)
{
    HintLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintLabel"));
    HintLabel->SetText(FText::FromString(
        TEXT("WASD 移動  空白 跳躍  U/I/O/P 施放  E 編輯器  C 數值  Z 背包  X 裝備  B 設定  N 形狀  V 技能組")));
    {
        FSlateFontInfo F = HintLabel->GetFont(); F.Size = 10; HintLabel->SetFont(F);
    }
    HintLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.45f, 0.50f)));
    HintLabel->SetJustification(ETextJustify::Center);
    Root->AddChild(HintLabel);
    Pin(HintLabel, { 0.f, -16.f }, { 800.f, 14.f },
        FAnchors(0.5f, 1.f, 0.5f, 1.f), { 0.5f, 1.f });
}

void UPlayerHUDWidget::BuildClock(UCanvasPanel* Root)
{
    ClockText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ClockText"));
    {
        FSlateFontInfo F = ClockText->GetFont(); F.Size = 12; ClockText->SetFont(F);
    }
    ClockText->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.88f, 0.75f)));
    ClockText->SetJustification(ETextJustify::Right);
    ClockText->SetText(FText::FromString(TEXT("蒼和5000年 01月01日 00：00 星期一")));
    Root->AddChild(ClockText);
    // 右上角，距右邊 10px，距頂 10px；小地圖佔位圓建後右移 ≈ 100px
    Pin(ClockText, { -100.f, 10.f }, { 280.f, 18.f },
        FAnchors(1.f, 0.f, 1.f, 0.f), { 1.f, 0.f });
}

void UPlayerHUDWidget::BuildMinimap(UCanvasPanel* Root)
{
    // 右上角小地圖佔位圓（無功能，僅 UI 佔位）
    UBorder* Circle = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MinimapCircle"));
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    Brush.OutlineSettings.CornerRadii  = FVector4(40.f, 40.f, 40.f, 40.f);  // 全圓角 = 圓形
    Brush.TintColor    = FSlateColor(FLinearColor(0.05f, 0.08f, 0.15f, 0.75f));
    Brush.OutlineSettings.Color = FSlateColor(FLinearColor(0.25f, 0.35f, 0.55f, 0.90f));
    Brush.OutlineSettings.Width = 1.5f;
    Circle->SetBrush(Brush);
    Circle->SetPadding(FMargin(0.f));
    Root->AddChild(Circle);
    Pin(Circle, { -90.f, 10.f }, { 80.f, 80.f },
        FAnchors(1.f, 0.f, 1.f, 0.f), FVector2D(1.f, 0.f));

    // 「地圖」提示文字（居中）
    UTextBlock* MapLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MinimapLabel"));
    {
        FSlateFontInfo F = MapLbl->GetFont(); F.Size = 10; MapLbl->SetFont(F);
    }
    MapLbl->SetText(FText::FromString(TEXT("地圖")));
    MapLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.30f, 0.40f, 0.55f)));
    MapLbl->SetJustification(ETextJustify::Center);
    Circle->AddChild(MapLbl);
    if (UBorderSlot* BS = Cast<UBorderSlot>(MapLbl->Slot))
    {
        BS->SetHorizontalAlignment(HAlign_Center);
        BS->SetVerticalAlignment(VAlign_Center);
    }
}

void UPlayerHUDWidget::BuildCarrySlot(UCanvasPanel* Root)
{
    // 拿取槽：圓形 48×48，熱鍵欄右側（StartX=66, 10 格×52=520, gap=8 → X=594）
    constexpr float SlotW = 48.f, SlotH = 48.f;
    constexpr float X = 594.f, Y = -132.f;
    constexpr float CornerR = SlotW * 0.5f;  // 全圓角 = 圓形

    CarryBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CarryBorder"));
    CarryBorder->SetPadding(FMargin(0.f));
    Root->AddChild(CarryBorder);
    PinBL(CarryBorder, { X, Y }, { SlotW, SlotH });

    // 內層 Canvas
    UCanvasPanel* SlotCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    CarryBorder->AddChild(SlotCanvas);
    if (UBorderSlot* BS = Cast<UBorderSlot>(SlotCanvas->Slot))
    {
        BS->SetHorizontalAlignment(HAlign_Fill);
        BS->SetVerticalAlignment(VAlign_Fill);
    }

    // 物品色塊圖示（居中）
    CarryIconBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    CarryIconBorder->SetBrush(MakeSolidBrush(FLinearColor(0.12f, 0.12f, 0.16f)));
    CarryIconBorder->SetPadding(FMargin(0.f));
    SlotCanvas->AddChild(CarryIconBorder);
    if (UCanvasPanelSlot* IS = Cast<UCanvasPanelSlot>(CarryIconBorder->Slot))
        IS->SetOffsets(FMargin(8.f, 8.f, 32.f, 32.f));

    // 「拿取」標籤（左上小字）
    UTextBlock* LabelTB = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    {
        FSlateFontInfo F = LabelTB->GetFont(); F.Size = 8; LabelTB->SetFont(F);
    }
    LabelTB->SetText(FText::FromString(TEXT("拿")));
    LabelTB->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.45f, 0.55f)));
    SlotCanvas->AddChild(LabelTB);
    if (UCanvasPanelSlot* KS = Cast<UCanvasPanelSlot>(LabelTB->Slot))
        KS->SetOffsets(FMargin(3.f, 2.f, 12.f, 10.f));

    // 初始化外框（空槽、非啟用態）
    UpdateCarrySlot(false, EItemId::None);
}

void UPlayerHUDWidget::UpdateCarrySlot(bool bIsCarrying, EItemId ItemId)
{
    constexpr float CornerR = 24.f;  // 48/2，全圓角 = 圓形

    if (CarryBorder)
    {
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        Brush.OutlineSettings.CornerRadii  = FVector4(CornerR, CornerR, CornerR, CornerR);
        Brush.TintColor = FSlateColor(bIsCarrying
            ? FLinearColor(0.18f, 0.18f, 0.28f)
            : FLinearColor(0.10f, 0.10f, 0.15f));
        Brush.OutlineSettings.Color = FSlateColor(bIsCarrying
            ? FLinearColor(0.95f, 0.80f, 0.20f)   // 攜帶中：金黃框
            : FLinearColor(0.35f, 0.45f, 0.60f));  // 空槽：藍灰框
        Brush.OutlineSettings.Width = bIsCarrying ? 2.f : 1.f;
        CarryBorder->SetBrush(Brush);
    }
    if (CarryIconBorder)
        CarryIconBorder->SetBrush(MakeSolidBrush(bIsCarrying && ItemId != EItemId::None
            ? ItemIconColor(ItemId)
            : FLinearColor(0.12f, 0.12f, 0.16f)));
}

// ══════════════════════════════════════════════════════════════════════════
// NativeTick — breakthrough fade-out + clock update
// ══════════════════════════════════════════════════════════════════════════

void UPlayerHUDWidget::NativeTick(const FGeometry& Geo, float Delta)
{
    Super::NativeTick(Geo, Delta);

    if (BreakthroughTimer > 0.f && BreakthroughLabel)
    {
        BreakthroughTimer -= Delta;
        float Alpha = FMath::Clamp(BreakthroughTimer, 0.f, 1.f);
        FLinearColor C = BreakthroughBaseColor;
        C.A = Alpha;
        BreakthroughLabel->SetColorAndOpacity(FSlateColor(C));
        if (BreakthroughTimer <= 0.f)
            BreakthroughLabel->SetVisibility(ESlateVisibility::Hidden);
    }

    // 時鐘：每遊戲分鐘更新一次（TicksPerMinute=20，1 分鐘=1 現實秒，每幀幾乎都要比對，
    // 但 SetText 代價高，用 LastClockMinute 只在分鐘跳動時才呼叫）
    if (ClockText)
    {
        if (UGameInstance* GI = GetGameInstance())
        if (UGameClockSubsystem* Clock = GI->GetSubsystem<UGameClockSubsystem>())
        {
            const int64 CurrentMinute = Clock->TotalTicks / UGameClockSubsystem::TicksPerMinute;
            if (CurrentMinute != LastClockMinute)
            {
                LastClockMinute = CurrentMinute;
                ClockText->SetText(FText::FromString(Clock->GetFormattedTime()));
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════
// Update methods (called by ASkillCreatorHUD::DrawHUD each frame)
// ══════════════════════════════════════════════════════════════════════════

void UPlayerHUDWidget::UpdateHpMp(float Hp, float MaxHp, float Mp, float MaxMp,
                                    float StaminaPct, float MoodPct)
{
    HpPercent      = MaxHp > 0.f ? FMath::Clamp(Hp / MaxHp, 0.f, 1.f) : 0.f;
    MpPercent      = MaxMp > 0.f ? FMath::Clamp(Mp / MaxMp, 0.f, 1.f) : 0.f;
    StaminaPercent = FMath::Clamp(StaminaPct, 0.f, 1.f);
    MoodPercent    = FMath::Clamp(MoodPct,    0.f, 1.f);  // 保留資料供 PlayerPanel 使用

    HpText = FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Hp, MaxHp));
    MpText = FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Mp, MaxMp));

    if (HpBar)       HpBar->SetPercent(HpPercent);
    if (MpBar)       MpBar->SetPercent(MpPercent);
    if (StaminaBar)  StaminaBar->SetPercent(StaminaPercent);
    if (HpTextBlock) HpTextBlock->SetText(HpText);
    if (MpTextBlock) MpTextBlock->SetText(MpText);
    if (HpLabel)     HpLabel->SetText(FText::FromString(
        FString::Printf(TEXT("HP  %.0f / %.0f"), Hp, MaxHp)));
    if (HpMpCircle)  HpMpCircle->UpdateHp(HpPercent, MpPercent);
}

void UPlayerHUDWidget::UpdateSpellHotBar(const TArray<FSpellArray>& HotBar, int32 ActiveIdx)
{
    const bool bSlotChanged = (ActiveIdx != ActiveSpellIndex);
    ActiveSpellIndex = ActiveIdx;

    HotBarNames.SetNum(HotBar.Num());
    for (int32 i = 0; i < HotBar.Num(); ++i)
        HotBarNames[i] = HotBar[i].Name.IsEmpty()
            ? FText::FromString(TEXT("—"))
            : FText::FromString(HotBar[i].Name);

    if (HotBarBox)
    {
        HotBarBox->ClearChildren();
        for (int32 i = 0; i < HotBarNames.Num(); ++i)
        {
            UTextBlock* TB = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            FString Label = FString::Printf(TEXT("[%d] %s%s"), i + 1,
                *HotBarNames[i].ToString(),
                i == ActiveIdx ? TEXT(" ◀") : TEXT(""));
            TB->SetText(FText::FromString(Label));
            TB->SetColorAndOpacity(FSlateColor(
                i == ActiveIdx
                    ? FLinearColor(1.f, 0.85f, 0.f)
                    : FLinearColor::White));
            HotBarBox->AddChild(TB);
        }
    }

    if (bSlotChanged)
        OnActiveSlotChanged(ActiveIdx);
}

void UPlayerHUDWidget::UpdateItemHotbar(const TArray<FItemStack>& Slots, int32 ActiveIdx)
{
    for (int32 i = 0; i < 10 && i < Slots.Num() && i < ItemSlotBorders.Num(); ++i)
    {
        bool bActive = (i == ActiveIdx);
        if (ItemSlotBorders[i])
        {
            // Bug H-2：原本只改 BrushColor（背景），active slot 的金黃邊框從未設定。
            // 對應 Godot RefreshHotbar() BorderColor=(0.95,0.80,0.20) + BorderWidth=2 for active slot。
            FSlateBrush Brush;
            Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
            Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
            Brush.OutlineSettings.CornerRadii  = FVector4(0, 0, 0, 0);
            Brush.TintColor = FSlateColor(bActive
                ? FLinearColor(0.18f, 0.18f, 0.28f)
                : FLinearColor(0.10f, 0.10f, 0.15f));
            Brush.OutlineSettings.Color = FSlateColor(bActive
                ? FLinearColor(0.95f, 0.80f, 0.20f)  // Godot 金黃邊框
                : FLinearColor(0.22f, 0.22f, 0.30f)); // 非 active 深灰邊框
            Brush.OutlineSettings.Width = bActive ? 2.f : 0.f;
            ItemSlotBorders[i]->SetBrush(Brush);
        }

        if (ItemIconBorders[i])
            ItemIconBorders[i]->SetBrush(MakeSolidBrush(Slots[i].IsEmpty()
                ? FLinearColor(0.18f, 0.18f, 0.22f)
                : ItemIconColor(Slots[i].ItemId)));

        if (ItemCountLabels[i])
            ItemCountLabels[i]->SetText(Slots[i].IsEmpty()
                ? FText::GetEmpty()
                : FText::FromString(FString::Printf(TEXT("×%d"), Slots[i].Count)));
    }
}

void UPlayerHUDWidget::UpdateOffhandSlot(const FItemStack& ItemSlot, bool bActive)
{
    constexpr float CornerR = 24.f;  // 48 / 2，全圓角 = 圓形外框

    if (OffhandBorder)
    {
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        Brush.OutlineSettings.CornerRadii  = FVector4(CornerR, CornerR, CornerR, CornerR);
        Brush.TintColor = FSlateColor(bActive
            ? FLinearColor(0.18f, 0.18f, 0.28f)
            : FLinearColor(0.10f, 0.10f, 0.15f));
        Brush.OutlineSettings.Color = FSlateColor(bActive
            ? FLinearColor(0.95f, 0.80f, 0.20f)   // 啟用：金黃邊框（對齊熱鍵欄 active 顏色）
            : FLinearColor(0.45f, 0.55f, 0.65f));  // 未啟用：藍灰邊框（區別於熱鍵欄深灰）
        Brush.OutlineSettings.Width = bActive ? 2.f : 1.5f;
        OffhandBorder->SetBrush(Brush);
    }

    if (OffhandIconBorder)
        OffhandIconBorder->SetBrush(MakeSolidBrush(ItemSlot.IsEmpty()
            ? FLinearColor(0.18f, 0.18f, 0.22f)
            : ItemIconColor(ItemSlot.ItemId)));

    if (OffhandCountLabel)
        OffhandCountLabel->SetText(ItemSlot.IsEmpty()
            ? FText::GetEmpty()
            : FText::FromString(FString::Printf(TEXT("×%d"), ItemSlot.Count)));
}

void UPlayerHUDWidget::UpdateSurvival(const UCharacterStateComponent* S)
{
    if (!S) return;

    // 4 條標準垂直條：飢餓 / 口渴 / 氧氣 / 體力（與 BuildSurvivalBars 順序一致）
    constexpr float BarW           = 10.f;
    constexpr float BarH           = 80.f;
    constexpr float GapX           = 18.f;
    constexpr float StartX         = 10.f;
    constexpr float BarBottomAbove = 150.f;

    struct { float V; float Max; bool Danger; } StdData[] = {
        { S->Hunger,  UCharacterStateComponent::MaxHunger,  S->IsStarving()        },
        { S->Thirst,  UCharacterStateComponent::MaxThirst,  S->IsDehydrated()      },
        { S->Oxygen,  UCharacterStateComponent::MaxOxygen,  S->IsSuffocating()     },
        { S->Stamina, UCharacterStateComponent::MaxStamina, S->IsStaminaDepleted() },
    };

    for (int32 i = 0; i < 4 && i < SurvivalBarFills.Num(); ++i)
    {
        float Ratio  = StdData[i].Max > 0.f ? FMath::Clamp(StdData[i].V / StdData[i].Max, 0.f, 1.f) : 0.f;
        float FillH  = BarH * Ratio;
        float FillY  = -(BarBottomAbove + FillH);  // 填充頂端 PinBL Y（底部對齊條底）
        float X      = StartX + i * GapX;

        if (SurvivalBarFills[i])
        {
            if (UCanvasPanelSlot* FS = Cast<UCanvasPanelSlot>(SurvivalBarFills[i]->Slot))
            {
                FS->SetSize(FVector2D(BarW, FillH));
                FS->SetPosition(FVector2D(X, FillY));
            }
        }
        if (i < SurvivalValLabels.Num() && SurvivalValLabels[i])
        {
            SurvivalValLabels[i]->SetColorAndOpacity(FSlateColor(
                StdData[i].Danger
                    ? FLinearColor(1.0f, 0.35f, 0.35f)
                    : FLinearColor(0.55f, 0.55f, 0.60f)));
        }
    }

    // ── 體溫雙向條 ──────────────────────────────────────────────────────
    // 熱端：NormalBodyTemp → HeatstrokeThreshold（5.5°C 範圍，往上填充）
    // 冷端：NormalBodyTemp → HypothermiaThreshold（26.5°C 範圍，往下填充）
    constexpr float Normal    = UCharacterStateComponent::NormalBodyTemp;
    constexpr float ColdRange = Normal - UCharacterStateComponent::HypothermiaThreshold;
    constexpr float HotRange  = UCharacterStateComponent::HeatstrokeThreshold - Normal;
    constexpr float TempBarCenterAbove = BarBottomAbove + BarH * 0.5f;  // 190px above screen
    constexpr float HalfH    = BarH * 0.5f;  // 40px (each half)
    constexpr float TempX    = StartX + 4.f * GapX;  // X=82

    const float T    = S->BodyTemperature;
    float HotF  = FMath::Clamp((T - Normal) / HotRange,  0.f, 1.f);
    float ColdF = FMath::Clamp((Normal - T) / ColdRange, 0.f, 1.f);
    float HotH  = HalfH * HotF;
    float ColdH = HalfH * ColdF;

    if (TempHotFill)
    {
        if (UCanvasPanelSlot* HS = Cast<UCanvasPanelSlot>(TempHotFill->Slot))
        {
            // 熱填充頂端從中線往上延伸：Y = -(center + HotH)
            HS->SetSize(FVector2D(BarW, HotH));
            HS->SetPosition(FVector2D(TempX, -(TempBarCenterAbove + HotH)));
        }
    }
    if (TempColdFill)
    {
        if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(TempColdFill->Slot))
        {
            // 冷填充頂端固定在中線，往下延伸：Y 固定 = -center
            CS->SetSize(FVector2D(BarW, ColdH));
            CS->SetPosition(FVector2D(TempX, -TempBarCenterAbove));
        }
    }
}

void UPlayerHUDWidget::UpdateLevelHUD(int32 Level, float Xp, int32 XpReq,
                                       const FString& TierName, FLinearColor TierColor)
{
    if (LevelLabel)
        LevelLabel->SetText(FText::FromString(FString::Printf(TEXT("LV %d"), Level)));

    if (TierLabel)
    {
        TierLabel->SetText(FText::FromString(FString::Printf(TEXT("【%s】"), *TierName)));
        TierLabel->SetColorAndOpacity(FSlateColor(TierColor));
    }

    if (XpLabel)
        XpLabel->SetText(FText::FromString(
            FString::Printf(TEXT("%.0f / %d XP"), Xp, XpReq)));

    if (XpBarFill)
    {
        float Ratio = XpReq > 0 ? FMath::Clamp(Xp / XpReq, 0.f, 1.f) : 1.f;
        if (UCanvasPanelSlot* XS = Cast<UCanvasPanelSlot>(XpBarFill->Slot))
        {
            // XP 條置中：填充從左端開始，offset 維持與背景相同（alignment=0.5,1），
            // 只改寬度；position 已由 BuildLevelHud 設為置中
            XS->SetSize(FVector2D(XpBarMaxWidth * Ratio, 8.f));
            // 填充須對齊背景左邊緣：background 左邊 = center - W/2，fill 左邊需一致
            // 改 alignment 為 (0,1) 後，調整 position 到背景左邊緣
            XS->SetAlignment(FVector2D(0.f, 1.f));
            XS->SetPosition(FVector2D(-XpBarMaxWidth * 0.5f, -72.f));
        }
    }
}

void UPlayerHUDWidget::UpdateEquipLabel(const FString& Summary)
{
    if (EquipLabel)
        EquipLabel->SetText(FText::FromString(Summary));
}

void UPlayerHUDWidget::UpdateManaSlots(const TArray<FManaSlot>& Slots)
{
    if (!ManaHudVBox) return;

    // 偵測 slot 組成改變 → 重建
    bool bNeedRebuild = (Slots.Num() != ManaCachedKeys.Num());
    if (!bNeedRebuild)
        for (int32 i = 0; i < ManaCachedKeys.Num(); ++i)
            if (ManaCachedKeys[i] != Slots[i].ManaTypeKey) { bNeedRebuild = true; break; }

    if (bNeedRebuild)
    {
        ManaHudVBox->ClearChildren();
        ManaBarFills.Empty();
        ManaValLabels.Empty();
        ManaCachedKeys.Empty();

        constexpr float BarW = 76.f, BarH = 8.f, RowH = 16.f;
        constexpr float LblW = 28.f, ValW = 48.f;

        for (const FManaSlot& S : Slots)
        {
            FLinearColor C = ManaColor(S.ManaTypeKey);

            UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
            if (UVerticalBoxSlot* VS = ManaHudVBox->AddChildToVerticalBox(Row))
                VS->SetPadding(FMargin(0.f, 1.f));

            UTextBlock* NmLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            NmLbl->SetText(FText::FromString(ManaAbbrev(S.ManaTypeKey)));
            {
                FSlateFontInfo F = NmLbl->GetFont(); F.Size = 10; NmLbl->SetFont(F);
            }
            NmLbl->SetColorAndOpacity(FSlateColor(C));
            if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(NmLbl))
                HS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

            // Bug H-4 修復：Fill 若放在 BarBg(UBorder)->AddChild() 裡，Slot=UBorderSlot，
            // Cast<UCanvasPanelSlot> null，UpdateManaSlots 的 SetSize 從未執行。
            // 改用 USizeBox(強制尺寸) + UCanvasPanel(絕對定位) + BarBg + Fill 結構，
            // Fill->Slot 型別為 UCanvasPanelSlot，可正常調整寬度。
            USizeBox* BarSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            BarSizeBox->SetWidthOverride(BarW);
            BarSizeBox->SetHeightOverride(BarH);
            if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(BarSizeBox))
            {
                HS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
                HS->SetPadding(FMargin(3.f, 0.f));
            }
            UCanvasPanel* BarCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
            BarSizeBox->AddChild(BarCanvas);

            UBorder* BarBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
            BarBg->SetBrush(MakeSolidBrush(FLinearColor(0.08f, 0.08f, 0.14f)));
            BarBg->SetPadding(FMargin(0.f));
            BarCanvas->AddChild(BarBg);
            if (UCanvasPanelSlot* BS = Cast<UCanvasPanelSlot>(BarBg->Slot))
            {
                BS->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
                BS->SetPosition(FVector2D::ZeroVector);
                BS->SetSize(FVector2D(BarW, BarH));
                BS->SetAlignment(FVector2D::ZeroVector);
            }

            UBorder* Fill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
            Fill->SetBrush(MakeSolidBrush(C));
            Fill->SetPadding(FMargin(0.f));
            BarCanvas->AddChild(Fill);
            if (UCanvasPanelSlot* FS = Cast<UCanvasPanelSlot>(Fill->Slot))
            {
                FS->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
                FS->SetPosition(FVector2D::ZeroVector);
                FS->SetSize(FVector2D(BarW, BarH));  // 初始滿格，UpdateManaSlots 按比例縮減
                FS->SetAlignment(FVector2D::ZeroVector);
            }
            ManaBarFills.Add(Fill);

            UTextBlock* Val = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            {
                FSlateFontInfo F = Val->GetFont(); F.Size = 9; Val->SetFont(F);
            }
            Val->SetColorAndOpacity(FSlateColor(FLinearColor(0.68f, 0.68f, 0.72f)));
            if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(Val))
                HS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
            ManaValLabels.Add(Val);

            ManaCachedKeys.Add(S.ManaTypeKey);
        }
    }

    // 更新每條數值
    constexpr float BarW = 76.f;
    for (int32 i = 0; i < Slots.Num() && i < ManaBarFills.Num(); ++i)
    {
        float Ratio = Slots[i].Max > 0.f
            ? FMath::Clamp(Slots[i].Current / Slots[i].Max, 0.f, 1.f) : 0.f;
        if (ManaBarFills[i])
        {
            if (UCanvasPanelSlot* FS = Cast<UCanvasPanelSlot>(ManaBarFills[i]->Slot))
                FS->SetSize(FVector2D(BarW * Ratio, 8.f));
        }
        if (ManaValLabels[i])
            ManaValLabels[i]->SetText(FText::FromString(
                FString::Printf(TEXT("%.0f/%.0f"), Slots[i].Current, Slots[i].Max)));
    }
    if (HpMpCircle) HpMpCircle->UpdateManaSlots(Slots);
}

void UPlayerHUDWidget::ShowDeathScreen(bool bVisible)
{
    if (DeathScreen)
        DeathScreen->SetVisibility(bVisible
            ? ESlateVisibility::Visible
            : ESlateVisibility::Hidden);
}

void UPlayerHUDWidget::ShowBreakthrough(const FString& TierName, FLinearColor Color)
{
    if (!BreakthroughLabel) return;
    BreakthroughLabel->SetText(FText::FromString(
        FString::Printf(TEXT("⬆ 境界突破：%s"), *TierName)));
    BreakthroughBaseColor = Color;
    BreakthroughLabel->SetColorAndOpacity(FSlateColor(Color));
    BreakthroughLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
    BreakthroughTimer = 3.f;
}

void UPlayerHUDWidget::SetActiveShape(EPlacementShape Shape, int32 Radius)
{
    if (!ShapeLabel) return;
    FString Names[] = { TEXT("單格"), TEXT("立方"), TEXT("球形"), TEXT("圓柱"), TEXT("平面") };
    int32 Idx = (int32)Shape;
    FString Nm = (Idx >= 0 && Idx < 5) ? Names[Idx] : TEXT("?");
    ShapeLabel->SetText(FText::FromString(
        FString::Printf(TEXT("形狀：%s（r=%d）"), *Nm, Radius)));
}

void UPlayerHUDWidget::ShowFloatTooltip(const FString& Text, FVector2D ScreenPos)
{
    if (!FloatTooltipPanel || !FloatTooltipText) return;
    FloatTooltipText->SetText(FText::FromString(Text));
    FloatTooltipPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (UCanvasPanelSlot* TS = Cast<UCanvasPanelSlot>(FloatTooltipPanel->Slot))
        TS->SetPosition(ScreenPos + FVector2D(14.f, -28.f));
}

void UPlayerHUDWidget::HideFloatTooltip()
{
    if (FloatTooltipPanel)
        FloatTooltipPanel->SetVisibility(ESlateVisibility::Hidden);
}

// ── Phase 4 新增：HP/MP 圓形水缸 ─────────────────────────────────────────────

void UPlayerHUDWidget::BuildHpMpCircle(UCanvasPanel* Root)
{
    HpMpCircle = CreateWidget<UHpMpCircleWidget>(GetOwningPlayer(),
        UHpMpCircleWidget::StaticClass());
    if (!HpMpCircle) return;
    Root->AddChild(HpMpCircle);
    Pin(HpMpCircle, { 10.f, 10.f }, { 120.f, 120.f });
}

// ── Phase 4 新增：異常狀態圖示列 ─────────────────────────────────────────────

void UPlayerHUDWidget::BuildAbnormalStatusBar(UCanvasPanel* Root)
{
    AbnormalStatusBar = CreateWidget<UAbnormalStatusBarWidget>(GetOwningPlayer(),
        UAbnormalStatusBarWidget::StaticClass());
    if (!AbnormalStatusBar) return;
    Root->AddChild(AbnormalStatusBar);
    // 頂部置中，距頂 10px，最多 15 格 (15*22=330px)，高 25px
    Pin(AbnormalStatusBar, { 0.f, 10.f }, { 330.f, 25.f },
        FAnchors(0.5f, 0.f, 0.5f, 0.f), FVector2D(0.5f, 0.f));
}

void UPlayerHUDWidget::UpdateAbnormalStatusBar(const TArray<FStatusDisplaySnapshot>& Snaps)
{
    if (AbnormalStatusBar) AbnormalStatusBar->UpdateStatuses(Snaps);
}
