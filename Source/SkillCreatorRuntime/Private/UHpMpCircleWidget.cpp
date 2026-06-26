#include "UHpMpCircleWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateLayoutTransform.h"

void UHpMpCircleWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    // 空 Root Canvas：讓引擎分配 120×120 幾何（由 HUD canvas slot 決定）
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;
}

void UHpMpCircleWidget::UpdateHp(float HpPct, float MpPct)
{
    HpPercent = FMath::Clamp(HpPct, 0.f, 1.f);
    MpPercent = FMath::Clamp(MpPct, 0.f, 1.f);
}

void UHpMpCircleWidget::UpdateManaSlots(const TArray<FManaSlot>& Slots)
{
    ManaSlots = Slots;
    if (Slots.Num() > 0 && Slots[0].Max > 0.f)
        MpPercent = FMath::Clamp(Slots[0].Current / Slots[0].Max, 0.f, 1.f);
}

// ── NativePaint ───────────────────────────────────────────────────────────────

int32 UHpMpCircleWidget::NativePaint(
    const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    const int32 Base = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
        OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    // Widget-local 座標（全部用 float 避免 double→float 警告）
    const FVector2f LS  = (FVector2f)AllottedGeometry.GetLocalSize();
    const FVector2f Ctr(LS.X * 0.5f, LS.Y * 0.5f);
    constexpr float HpR    = 44.f;
    constexpr float MpMidR = 52.f;
    constexpr float MpThick= 8.f;

    // ── 1. 水缸掃描線 ────────────────────────────────────────────────────────
    const FLinearColor BgCol(0.04f, 0.02f, 0.02f, 0.92f);
    const FLinearColor WaCol = HpPercent > 0.25f
        ? FLinearColor(0.68f, 0.13f, 0.13f, 0.88f)
        : FLinearColor(0.88f, 0.28f, 0.05f, 0.88f);

    const float WaterTopY = Ctr.Y + HpR - HpPercent * 2.f * HpR;

    FSlateBrush BgBr, WaBr;
    BgBr.DrawAs = ESlateBrushDrawType::Image;
    WaBr.DrawAs = ESlateBrushDrawType::Image;

    for (float y = Ctr.Y - HpR; y <= Ctr.Y + HpR + 0.5f; y += 1.f)
    {
        const float dy    = y - Ctr.Y;
        const float halfW = FMath::Sqrt(FMath::Max(0.f, HpR * HpR - dy * dy));
        if (halfW < 0.5f) continue;

        const bool bWater = (y >= WaterTopY);
        FSlateDrawElement::MakeBox(
            OutDrawElements, Base,
            AllottedGeometry.ToPaintGeometry(
                FVector2D(halfW * 2.0, 1.5),
                FSlateLayoutTransform(FVector2f(Ctr.X - halfW, y))),
            bWater ? &WaBr : &BgBr,
            ESlateDrawEffect::None,
            bWater ? WaCol : BgCol);
    }

    // ── 2. 水面高光線 ─────────────────────────────────────────────────────────
    if (HpPercent > 0.01f && HpPercent < 0.99f)
    {
        const float dy    = WaterTopY - Ctr.Y;
        const float halfW = FMath::Sqrt(FMath::Max(0.f, HpR * HpR - dy * dy));
        if (halfW > 1.f)
        {
            TArray<FVector2D> Surf = {
                FVector2D(Ctr.X - halfW, WaterTopY),
                FVector2D(Ctr.X + halfW, WaterTopY) };
            FSlateDrawElement::MakeLines(OutDrawElements, Base + 1,
                AllottedGeometry.ToPaintGeometry(), Surf,
                ESlateDrawEffect::None, FLinearColor(1.f, 0.7f, 0.7f, 0.55f), false, 1.5f);
        }
    }

    // ── 3. HP 圓形外框 ────────────────────────────────────────────────────────
    {
        constexpr int32 Seg = 64;
        TArray<FVector2D> Pts;
        Pts.Reserve(Seg + 1);
        for (int32 i = 0; i <= Seg; i++)
        {
            const float a = (float)i / Seg * 2.f * PI;
            Pts.Add(FVector2D(Ctr.X + FMath::Cos(a) * HpR, Ctr.Y + FMath::Sin(a) * HpR));
        }
        FSlateDrawElement::MakeLines(OutDrawElements, Base + 2,
            AllottedGeometry.ToPaintGeometry(), Pts,
            ESlateDrawEffect::None, FLinearColor(0.75f, 0.75f, 0.75f, 0.65f), true, 1.5f);
    }

    // ── 4. MP 外環弧段 ────────────────────────────────────────────────────────
    const int32 NumSegs = FMath::Max(1, ManaSlots.Num());
    constexpr float GapRad = 0.06f;
    const float     SegArc = (2.f * PI - NumSegs * GapRad) / NumSegs;
    const FLinearColor DarkSeg(0.08f, 0.08f, 0.18f, 0.65f);
    const FVector2D CtrD((double)Ctr.X, (double)Ctr.Y);

    for (int32 i = 0; i < NumSegs; i++)
    {
        const float A0 = -PI * 0.5f + i * (SegArc + GapRad);
        const float A1 = A0 + SegArc;

        // 暗底
        DrawArcSeg(OutDrawElements, Base + 2, AllottedGeometry, CtrD, MpMidR, MpThick, A0, A1, DarkSeg);

        // 填充（從 segment 0 往後消耗）
        const float Fill = FMath::Clamp(MpPercent * NumSegs - i, 0.f, 1.f);
        if (Fill > 0.01f)
            DrawArcSeg(OutDrawElements, Base + 3, AllottedGeometry, CtrD, MpMidR, MpThick,
                A0, A0 + SegArc * Fill, SegColor(i, ManaSlots));
    }

    return Base + 4;
}

// ── 靜態 helper ───────────────────────────────────────────────────────────────

FLinearColor UHpMpCircleWidget::SegColor(int32 Idx, const TArray<FManaSlot>& Slots)
{
    if (!Slots.IsValidIndex(Idx)) return FLinearColor(0.20f, 0.40f, 0.95f, 0.90f);
    const FName K = Slots[Idx].ManaTypeKey;
    if (K == "gui_dao")    return FLinearColor(0.20f, 0.40f, 0.95f, 0.90f);
    if (K == "fire_mana")  return FLinearColor(0.95f, 0.30f, 0.10f, 0.90f);
    if (K == "ice_mana")   return FLinearColor(0.40f, 0.80f, 1.00f, 0.90f);
    if (K == "earth_mana") return FLinearColor(0.55f, 0.40f, 0.20f, 0.90f);
    if (K == "wind_mana")  return FLinearColor(0.60f, 0.90f, 0.55f, 0.90f);
    return FLinearColor(0.20f, 0.40f, 0.95f, 0.90f);
}

void UHpMpCircleWidget::DrawArcSeg(
    FSlateWindowElementList& Out, int32 Layer,
    const FGeometry& Geo, FVector2D Center, float MidR, float Thickness,
    float A0, float A1, FLinearColor Col)
{
    if (A1 <= A0 + 0.001f) return;
    const int32 Steps = FMath::Max(2, (int32)((A1 - A0) / (2.f * PI) * 64.f));
    TArray<FVector2D> Pts;
    Pts.Reserve(Steps + 1);
    for (int32 s = 0; s <= Steps; s++)
    {
        const float a = A0 + (A1 - A0) * (float)s / Steps;
        Pts.Add(FVector2D(Center.X + FMath::Cos(a) * MidR, Center.Y + FMath::Sin(a) * MidR));
    }
    FSlateDrawElement::MakeLines(Out, Layer,
        Geo.ToPaintGeometry(), Pts,
        ESlateDrawEffect::None, Col, true, Thickness);
}
