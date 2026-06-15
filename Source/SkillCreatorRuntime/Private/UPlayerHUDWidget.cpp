#include "UPlayerHUDWidget.h"
#include "Components/CanvasPanelSlot.h"

// Helper: set Canvas Panel slot layout in one call.
static void ApplySlot(UWidget* W, FVector2D Pos, FVector2D Size,
                      FAnchors Anchors, FVector2D Alignment = FVector2D::ZeroVector)
{
    if (!W) return;
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(W->Slot))
    {
        S->SetAnchors(Anchors);
        S->SetPosition(Pos);
        S->SetSize(Size);
        S->SetAlignment(Alignment);
    }
}

void UPlayerHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // ── Left side: HP / MP / Stamina / Mood (anchor = top-left) ──
    ApplySlot(HpBar,        { 20,  20}, {200, 18}, FAnchors(0, 0));
    ApplySlot(HpTextBlock,  {228,  20}, { 90, 18}, FAnchors(0, 0));
    ApplySlot(MpBar,        { 20,  46}, {200, 18}, FAnchors(0, 0));
    ApplySlot(MpTextBlock,  {228,  46}, { 90, 18}, FAnchors(0, 0));
    ApplySlot(StaminaBar,   { 20,  72}, {200, 14}, FAnchors(0, 0));
    ApplySlot(MoodBar,      { 20,  94}, {200, 14}, FAnchors(0, 0));

    // ── Bottom centre: HotBar (anchor = bottom-centre) ──
    //   Alignment (0.5, 1) = pivot at bottom-centre of the box itself.
    ApplySlot(HotBarBox,    {  0, -10}, {400, 48}, FAnchors(0.5f, 1.f), {0.5f, 1.f});

    // ── Right side: ActiveSlotList (anchor = top-right) ──
    //   Alignment (1, 0) = pivot at top-right of the box.
    ApplySlot(ActiveSlotBox,{-20,  20}, {180,200}, FAnchors(1.f, 0.f),  {1.f, 0.f});
}

void UPlayerHUDWidget::UpdateHpMp(float Hp, float MaxHp, float Mp, float MaxMp,
                                    float StaminaPct, float MoodPct)
{
    HpPercent     = MaxHp > 0.f ? FMath::Clamp(Hp / MaxHp, 0.f, 1.f) : 0.f;
    MpPercent     = MaxMp > 0.f ? FMath::Clamp(Mp / MaxMp, 0.f, 1.f) : 0.f;
    StaminaPercent = FMath::Clamp(StaminaPct, 0.f, 1.f);
    MoodPercent    = FMath::Clamp(MoodPct,    0.f, 1.f);

    // Format "CurrentInt / MaxInt" — no decimal for whole numbers.
    HpText = FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Hp, MaxHp));
    MpText = FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Mp, MaxMp));
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

    if (bSlotChanged)
        OnActiveSlotChanged(ActiveIdx);
}
