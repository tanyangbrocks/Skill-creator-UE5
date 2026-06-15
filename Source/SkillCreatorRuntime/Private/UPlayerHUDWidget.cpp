#include "UPlayerHUDWidget.h"

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
