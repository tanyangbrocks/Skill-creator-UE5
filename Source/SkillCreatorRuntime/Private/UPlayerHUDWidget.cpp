#include "UPlayerHUDWidget.h"
#include "ASkillCreatorHUD.h"
#include "ASkillCreatorCharacter.h"
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

// ── 純色筆刷輔助（UBorder 只有 SetBrush({RoundedBox,TintColor}) 才保證無貼圖渲染，
//    SetBrushColor 依賴 T_DefaultDiffuse_D 存在，UE5.7 失效 → 全改走此路徑）
static FSlateBrush MakeSolid(FLinearColor C)
{
    FSlateBrush B;
    B.DrawAs   = ESlateBrushDrawType::RoundedBox;
    B.TintColor = FSlateColor(C);
    return B;
}

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

    // 如果 Blueprint 子類已綁定 widget，僅調整位置後退出
    if (HpBar)
    {
        Pin(HpBar,        { 20, 20 }, { 180, 16 });
        Pin(HpTextBlock,  { 208, 20 }, { 120, 16 });
        Pin(MpBar,        { 20, 44 }, { 180, 16 });
        Pin(MpTextBlock,  { 208, 44 }, { 120, 16 });
        Pin(StaminaBar,   { 20, 68 }, { 180, 12 });
        Pin(MoodBar,      { 20, 88 }, { 180, 12 });
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
    MoodBar    = AddBar(TEXT("MoodBar"),    FLinearColor(0.65f, 0.20f, 0.80f), { 20, 88 });
    HpTextBlock = AddTxt(TEXT("HpText"), { 208, 20 });
    MpTextBlock = AddTxt(TEXT("MpText"), { 208, 44 });

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

    // ④ 物品熱鍵欄（底部 10 槽）
    BuildItemHotbar(Root);

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
    PinBL(ShapeLabel, { 10.f, -155.f }, { 160.f, 14.f });
    SetActiveShape(EPlacementShape::Single, 1);

    // ⑩ 提示文字（底部中央）
    BuildHintLabel(Root);

    // ⑪ 死亡遮罩
    BuildDeathScreenOverlay(Root);

    // ⑫ 境界突破通知
    BuildBreakthroughLabel(Root);

    // ⑬ 浮動 Tooltip
    BuildFloatTooltip(Root);
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
        B->SetBrush(MakeSolid(P.Color));
        B->SetPadding(FMargin(0.f));
        Root->AddChild(B);
        Pin(B, P.Pos, P.Size,
            FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D::ZeroVector);
    }
}

void UPlayerHUDWidget::BuildItemHotbar(UCanvasPanel* Root)
{
    constexpr float SlotW = 48.f, SlotH = 48.f, Gap = 4.f;
    constexpr float StartX = 10.f, StartY = -132.f;
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
        Icon->SetBrush(MakeSolid(FLinearColor(0.18f, 0.18f, 0.22f)));
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
    constexpr float StartX = 10.f, StartY = -248.f, RowH = 13.f;
    constexpr float LblW = 38.f, BarW = 74.f, BarH = 6.f, ValW = 36.f;
    SurvivalBarWidth = BarW;

    static const struct { const TCHAR* Name; FLinearColor Fill; } Defs[] = {
        { TEXT("體力"), FLinearColor(1.00f, 0.60f, 0.22f) },
        { TEXT("精力"), FLinearColor(0.65f, 0.35f, 1.00f) },
        { TEXT("心情"), FLinearColor(1.00f, 0.55f, 0.65f) },
        { TEXT("口渴"), FLinearColor(0.28f, 0.78f, 1.00f) },
        { TEXT("飢餓"), FLinearColor(0.60f, 0.88f, 0.22f) },
        { TEXT("氧氣"), FLinearColor(0.45f, 0.65f, 1.00f) },
    };
    constexpr int32 NBar = UE_ARRAY_COUNT(Defs);
    SurvivalBarFills.Reserve(NBar);
    SurvivalValLabels.Reserve(NBar);

    for (int32 i = 0; i < NBar; ++i)
    {
        float Y = StartY + i * RowH;

        UTextBlock* NmLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        NmLbl->SetText(FText::FromString(Defs[i].Name));
        {
            FSlateFontInfo F = NmLbl->GetFont(); F.Size = 10; NmLbl->SetFont(F);
        }
        NmLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.60f)));
        Root->AddChild(NmLbl);
        PinBL(NmLbl, { StartX, Y }, { LblW, RowH });

        UBorder* BarBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        BarBg->SetBrush(MakeSolid(FLinearColor(0.10f, 0.10f, 0.16f)));
        BarBg->SetPadding(FMargin(0.f));
        Root->AddChild(BarBg);
        const float FillX = StartX + LblW + 2.f, FillY = Y + (RowH - BarH) * 0.5f;
        PinBL(BarBg, { FillX, FillY }, { BarW, BarH });

        // Fill 直接加到 Root（與 BarBg 同位置，因此 Root 的渲染順序讓 Fill 蓋在 BarBg 上）。
        // Bug H-4 修復：Fill 若放在 BarBg->AddChild() 裡，Slot 型別是 UBorderSlot，
        // Cast<UCanvasPanelSlot> 永遠 null，UpdateSurvival 的 SetSize 從未執行。
        UBorder* Fill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Fill->SetBrush(MakeSolid(Defs[i].Fill));
        Fill->SetPadding(FMargin(0.f));
        Root->AddChild(Fill);
        PinBL(Fill, { FillX, FillY }, { BarW, BarH });
        SurvivalBarFills.Add(Fill);

        UTextBlock* Val = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        {
            FSlateFontInfo F = Val->GetFont(); F.Size = 10; Val->SetFont(F);
        }
        Val->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.70f)));
        Val->SetJustification(ETextJustify::Right);
        Root->AddChild(Val);
        PinBL(Val, { StartX + LblW + 2.f + BarW + 3.f, Y }, { ValW, RowH });
        SurvivalValLabels.Add(Val);
    }

    // 體溫（最後一行）
    float TempY = StartY + NBar * RowH;
    UTextBlock* TempNm = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TempNm->SetText(FText::FromString(TEXT("體溫")));
    {
        FSlateFontInfo F = TempNm->GetFont(); F.Size = 10; TempNm->SetFont(F);
    }
    TempNm->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.60f)));
    Root->AddChild(TempNm);
    PinBL(TempNm, { StartX, TempY }, { LblW, RowH });

    BodyTempLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    {
        FSlateFontInfo F = BodyTempLabel->GetFont(); F.Size = 10; BodyTempLabel->SetFont(F);
    }
    BodyTempLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.80f, 0.75f)));
    Root->AddChild(BodyTempLabel);
    PinBL(BodyTempLabel, { StartX + LblW + 2.f, TempY }, { 84.f, RowH });
}

void UPlayerHUDWidget::BuildLevelHud(UCanvasPanel* Root)
{
    // 裝備標籤 y=-290
    EquipLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EquipLabel"));
    {
        FSlateFontInfo F = EquipLabel->GetFont(); F.Size = 11; EquipLabel->SetFont(F);
    }
    EquipLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.85f, 0.65f)));
    Root->AddChild(EquipLabel);
    PinBL(EquipLabel, { 10.f, -290.f }, { 300.f, 14.f });

    // LV 標籤 y=-275
    LevelLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LvLabel"));
    {
        FSlateFontInfo F = LevelLabel->GetFont(); F.Size = 13; LevelLabel->SetFont(F);
    }
    LevelLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.25f)));
    Root->AddChild(LevelLabel);
    PinBL(LevelLabel, { 10.f, -275.f }, { 48.f, 16.f });

    // 境界標籤 y=-275 x=58
    TierLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TierLabel"));
    {
        FSlateFontInfo F = TierLabel->GetFont(); F.Size = 13; TierLabel->SetFont(F);
    }
    TierLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)));
    Root->AddChild(TierLabel);
    PinBL(TierLabel, { 58.f, -275.f }, { 120.f, 16.f });

    // XP 文字 y=-274 x=180
    XpLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("XpLabel"));
    {
        FSlateFontInfo F = XpLabel->GetFont(); F.Size = 11; XpLabel->SetFont(F);
    }
    XpLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.60f)));
    Root->AddChild(XpLabel);
    PinBL(XpLabel, { 180.f, -274.f }, { 130.f, 14.f });

    // XP 條背景 y=-262 h=7
    UBorder* XpBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("XpBg"));
    XpBg->SetBrush(MakeSolid(FLinearColor(0.12f, 0.12f, 0.18f)));
    XpBg->SetPadding(FMargin(0.f));
    Root->AddChild(XpBg);
    PinBL(XpBg, { 10.f, -262.f }, { XpBarMaxWidth, 7.f });

    // XP 填充（Bug H-4 修復：直接加到 Root 而非 XpBg->AddChild，
    // 確保 Slot 型別是 UCanvasPanelSlot，UpdateLevelHUD 可呼叫 SetSize 調整進度）
    XpBarFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("XpFill"));
    XpBarFill->SetBrush(MakeSolid(FLinearColor(0.25f, 0.65f, 0.95f)));
    XpBarFill->SetPadding(FMargin(0.f));
    Root->AddChild(XpBarFill);
    PinBL(XpBarFill, { 10.f, -262.f }, { 0.f, 7.f });  // 初始寬度 0，由 UpdateLevelHUD 設
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
    DeathScreen->SetBrush(MakeSolid(FLinearColor(0.f, 0.f, 0.f, 0.72f)));
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

    UTextBlock* RespawnHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    RespawnHint->SetText(FText::FromString(TEXT("（前往遊戲流程選單重生）")));
    {
        FSlateFontInfo F = RespawnHint->GetFont(); F.Size = 15; RespawnHint->SetFont(F);
    }
    RespawnHint->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.80f)));
    RespawnHint->SetJustification(ETextJustify::Center);
    VBox->AddChildToVerticalBox(RespawnHint);
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
    FloatTooltipPanel->SetBrush(MakeSolid(FLinearColor(0.05f, 0.05f, 0.12f, 0.95f)));
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

// ══════════════════════════════════════════════════════════════════════════
// NativeTick — breakthrough fade-out
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

    // K-22 + hover tooltip：偵測游標在熱鍵欄格上，寫入 bMouseOverHotbar + 顯示物品名稱
    // （對應 Godot Main.cs:831-836 panel.MouseEntered → _mouseOverHotbar=true + ShowTooltip(idx)）
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (ASkillCreatorHUD* HUD = PC->GetHUD<ASkillCreatorHUD>())
        {
            float MX, MY;
            PC->GetMousePosition(MX, MY);
            FVector2D MouseAbs(MX, MY);

            int32 HoveredIdx = -1;
            for (int32 i = 0; i < ItemSlotBorders.Num(); ++i)
            {
                UBorder* B = ItemSlotBorders[i];
                if (!B) continue;
                const FGeometry& G = B->GetCachedGeometry();
                const FVector2D Local = G.AbsoluteToLocal(MouseAbs);
                const FVector2D Sz    = G.GetLocalSize();
                if (Local.X >= 0.f && Local.X <= Sz.X && Local.Y >= 0.f && Local.Y <= Sz.Y)
                { HoveredIdx = i; break; }
            }
            HUD->bMouseOverHotbar = (HoveredIdx >= 0);

            // Tooltip（對應 Godot ShowTooltip(idx) → ShowFloatTooltip(DisplayName)）
            if (HoveredIdx >= 0)
            {
                ASkillCreatorCharacter* Char = Cast<ASkillCreatorCharacter>(PC->GetPawn());
                const bool bHasItem = Char && Char->InventoryComp
                    && Char->InventoryComp->Slots.IsValidIndex(HoveredIdx)
                    && !Char->InventoryComp->Slots[HoveredIdx].IsEmpty();
                if (bHasItem)
                    ShowFloatTooltip(
                        FItemRegistry::Get(Char->InventoryComp->Slots[HoveredIdx].ItemId)
                            .DisplayName.ToString(),
                        MouseAbs);
                else
                    HideFloatTooltip();
            }
            else
            {
                HideFloatTooltip();
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
    MoodPercent    = FMath::Clamp(MoodPct,    0.f, 1.f);

    HpText = FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Hp, MaxHp));
    MpText = FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Mp, MaxMp));

    if (HpBar)       HpBar->SetPercent(HpPercent);
    if (MpBar)       MpBar->SetPercent(MpPercent);
    if (StaminaBar)  StaminaBar->SetPercent(StaminaPercent);
    if (MoodBar)     MoodBar->SetPercent(MoodPercent);
    if (HpTextBlock) HpTextBlock->SetText(HpText);
    if (MpTextBlock) MpTextBlock->SetText(MpText);
    if (HpLabel)     HpLabel->SetText(FText::FromString(
        FString::Printf(TEXT("HP  %.0f / %.0f"), Hp, MaxHp)));
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
            ItemIconBorders[i]->SetBrush(MakeSolid(Slots[i].IsEmpty()
                ? FLinearColor(0.18f, 0.18f, 0.22f)
                : ItemIconColor(Slots[i].ItemId)));

        if (ItemCountLabels[i])
            ItemCountLabels[i]->SetText(Slots[i].IsEmpty()
                ? FText::GetEmpty()
                : FText::FromString(FString::Printf(TEXT("×%d"), Slots[i].Count)));
    }
}

void UPlayerHUDWidget::UpdateSurvival(const UCharacterStateComponent* S)
{
    if (!S) return;

    struct { float V; float Max; bool Danger; } Data[] = {
        { S->Stamina,     UCharacterStateComponent::MaxStamina,     S->IsStaminaDepleted() },
        { S->MentalEnergy,UCharacterStateComponent::MaxMentalEnergy,S->IsMentalEnergyDepleted() },
        { S->Mood,        UCharacterStateComponent::MaxMood,         S->IsInsane() },
        { S->Thirst,      UCharacterStateComponent::MaxThirst,       S->IsDehydrated() },
        { S->Hunger,      UCharacterStateComponent::MaxHunger,       S->IsStarving() },
        { S->Oxygen,      UCharacterStateComponent::MaxOxygen,       S->IsSuffocating() },
    };

    for (int32 i = 0; i < UE_ARRAY_COUNT(Data) && i < SurvivalBarFills.Num(); ++i)
    {
        float Ratio = Data[i].Max > 0.f ? FMath::Clamp(Data[i].V / Data[i].Max, 0.f, 1.f) : 0.f;
        if (SurvivalBarFills[i])
        {
            if (UCanvasPanelSlot* FS = Cast<UCanvasPanelSlot>(SurvivalBarFills[i]->Slot))
                FS->SetSize(FVector2D(SurvivalBarWidth * Ratio, 6.f));
        }
        if (SurvivalValLabels[i])
        {
            SurvivalValLabels[i]->SetText(FText::FromString(
                FString::Printf(TEXT("%.0f"), Data[i].V)));
            SurvivalValLabels[i]->SetColorAndOpacity(FSlateColor(
                Data[i].Danger
                    ? FLinearColor(1.0f, 0.35f, 0.35f)
                    : FLinearColor(0.65f, 0.65f, 0.70f)));
        }
    }

    if (BodyTempLabel)
    {
        float T = S->BodyTemperature;
        FLinearColor TempCol = S->IsHypothermic() ? FLinearColor(0.40f, 0.65f, 1.00f)
                             : S->IsHeatstroke()  ? FLinearColor(1.00f, 0.40f, 0.20f)
                             :                      FLinearColor(0.80f, 0.80f, 0.75f);
        BodyTempLabel->SetText(FText::FromString(FString::Printf(TEXT("%.1f°C"), T)));
        BodyTempLabel->SetColorAndOpacity(FSlateColor(TempCol));
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
            XS->SetSize(FVector2D(XpBarMaxWidth * Ratio, 7.f));
    }
}

void UPlayerHUDWidget::UpdateEquipLabel(const FString& Weapon, const FString& Armor, const FString& Accessory)
{
    if (EquipLabel)
        EquipLabel->SetText(FText::FromString(
            FString::Printf(TEXT("W:%s  A:%s  飾:%s"), *Weapon, *Armor, *Accessory)));
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
            BarBg->SetBrush(MakeSolid(FLinearColor(0.08f, 0.08f, 0.14f)));
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
            Fill->SetBrush(MakeSolid(C));
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
