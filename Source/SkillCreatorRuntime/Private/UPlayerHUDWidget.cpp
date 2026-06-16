#include "UPlayerHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"

// ──────────────────────────────────────────────────────────────────────────
// Internal helpers
// ──────────────────────────────────────────────────────────────────────────

static void PinSlot(UWidget* W, FVector2D Pos, FVector2D Size,
                    FAnchors Anchors = FAnchors(0, 0),
                    FVector2D Align  = FVector2D::ZeroVector)
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

static UProgressBar* AddBar(UWidgetTree* WT, UCanvasPanel* Root, FName Name,
                             FLinearColor Color, FVector2D Pos)
{
    UProgressBar* Bar = WT->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
    Bar->SetFillColorAndOpacity(Color);
    Root->AddChild(Bar);
    PinSlot(Bar, Pos, {180, 16});
    return Bar;
}

static UTextBlock* AddText(UWidgetTree* WT, UCanvasPanel* Root, FName Name, FVector2D Pos)
{
    UTextBlock* TB = WT->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    TB->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    Root->AddChild(TB);
    PinSlot(TB, Pos, {120, 16});
    return TB;
}

// ──────────────────────────────────────────────────────────────────────────
// NativeConstruct
// ──────────────────────────────────────────────────────────────────────────

void UPlayerHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // If a Blueprint subclass already bound the widgets, just reposition them.
    if (HpBar)
    {
        PinSlot(HpBar,        {20, 20},  {180, 16});
        PinSlot(HpTextBlock,  {208, 20}, {120, 16});
        PinSlot(MpBar,        {20, 44},  {180, 16});
        PinSlot(MpTextBlock,  {208, 44}, {120, 16});
        PinSlot(StaminaBar,   {20, 68},  {180, 12});
        PinSlot(MoodBar,      {20, 88},  {180, 12});
        PinSlot(HotBarBox,    {0, -60},  {400, 48},
                FAnchors(0.5f, 1.f, 0.5f, 1.f), {0.5f, 1.f});
        return;
    }

    // ── Pure C++ widget tree (no WBP_PlayerHUD Blueprint required) ─────────
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    HpBar      = AddBar(WidgetTree, Root, TEXT("HpBar"),
                        FLinearColor(0.85f, 0.12f, 0.12f), {20, 20});
    MpBar      = AddBar(WidgetTree, Root, TEXT("MpBar"),
                        FLinearColor(0.15f, 0.45f, 0.90f), {20, 44});
    StaminaBar = AddBar(WidgetTree, Root, TEXT("StaminaBar"),
                        FLinearColor(0.15f, 0.80f, 0.25f), {20, 68});
    MoodBar    = AddBar(WidgetTree, Root, TEXT("MoodBar"),
                        FLinearColor(0.65f, 0.20f, 0.80f), {20, 88});

    HpTextBlock = AddText(WidgetTree, Root, TEXT("HpText"), {208, 20});
    MpTextBlock = AddText(WidgetTree, Root, TEXT("MpText"), {208, 44});

    // Hotbar VBox at bottom centre
    HotBarBox = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("HotBarBox"));
    Root->AddChild(HotBarBox);
    PinSlot(HotBarBox, {0, -60}, {400, 48},
            FAnchors(0.5f, 1.f, 0.5f, 1.f), {0.5f, 1.f});
}

// ──────────────────────────────────────────────────────────────────────────
// UpdateHpMp
// ──────────────────────────────────────────────────────────────────────────

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
}

// ──────────────────────────────────────────────────────────────────────────
// UpdateSpellHotBar
// ──────────────────────────────────────────────────────────────────────────

void UPlayerHUDWidget::UpdateSpellHotBar(const TArray<FSpellArray>& HotBar, int32 ActiveIdx)
{
    const bool bSlotChanged = (ActiveIdx != ActiveSpellIndex);
    ActiveSpellIndex = ActiveIdx;

    HotBarNames.SetNum(HotBar.Num());
    for (int32 i = 0; i < HotBar.Num(); ++i)
        HotBarNames[i] = HotBar[i].Name.IsEmpty()
            ? FText::FromString(TEXT("—"))
            : FText::FromString(HotBar[i].Name);

    ActiveSlotList.Empty();
    if (HotBar.IsValidIndex(ActiveIdx))
    {
        for (const FSpellSlot& S : HotBar[ActiveIdx].Slots)
        {
            FString Display = S.TotemId.IsNone()
                ? TEXT("(empty)")
                : S.TotemId.ToString();
            if (!S.Name.IsNone())
                Display = S.Name.ToString() + TEXT(": ") + Display;
            ActiveSlotList.Add(FText::FromString(Display));
        }
    }

    // Rebuild HotBarBox text children
    if (HotBarBox)
    {
        HotBarBox->ClearChildren();
        for (int32 i = 0; i < HotBarNames.Num(); ++i)
        {
            UTextBlock* TB = WidgetTree->ConstructWidget<UTextBlock>(
                UTextBlock::StaticClass());
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
