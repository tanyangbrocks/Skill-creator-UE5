#include "UPhysicalThrowWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"

void UPhysicalThrowWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    APlayerController* PC = GetOwningPlayer();

    Root = NewObject<UCanvasPanel>(this);
    WidgetTree->RootWidget = Root;

    // 背景條
    BarBackground = NewObject<UBorder>(this);
    BarBackground->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.8f));
    UCanvasPanelSlot* BgSlot = Root->AddChildToCanvas(BarBackground);
    BgSlot->SetSize(FVector2D(BarW, BarH));
    BgSlot->SetPosition(FVector2D(-BarW - 20.f, -(BarH * 0.5f)));  // 右側，垂直居中
    BgSlot->SetAnchors(FAnchors(1.f, 0.5f, 1.f, 0.5f));
    BgSlot->SetAlignment(FVector2D(1.f, 0.5f));

    // 填充條（從下往上，覆蓋在背景上）
    BarFill = NewObject<UBorder>(this);
    BarFill->SetBrushColor(FLinearColor(0.3f, 0.7f, 1.f, 0.9f));
    UCanvasPanelSlot* FillSlot = Root->AddChildToCanvas(BarFill);
    FillSlot->SetSize(FVector2D(BarW, 0.f));  // 初始高度 0，由 UpdatePointerPosition 設
    FillSlot->SetPosition(FVector2D(-BarW - 20.f, -(BarH * 0.5f)));
    FillSlot->SetAnchors(FAnchors(1.f, 0.5f, 1.f, 0.5f));
    FillSlot->SetAlignment(FVector2D(1.f, 0.5f));

    // 指針（白色細橫線）
    Pointer = NewObject<UBorder>(this);
    Pointer->SetBrushColor(FLinearColor::White);
    UCanvasPanelSlot* PtrSlot = Root->AddChildToCanvas(Pointer);
    PtrSlot->SetSize(FVector2D(BarW + 6.f, 3.f));
    PtrSlot->SetPosition(FVector2D(-BarW - 23.f, -(BarH * 0.5f) + BarH - 3.f));
    PtrSlot->SetAnchors(FAnchors(1.f, 0.5f, 1.f, 0.5f));
    PtrSlot->SetAlignment(FVector2D(1.f, 0.5f));

    SetVisibility(ESlateVisibility::Collapsed);
}

void UPhysicalThrowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bCharging) return;

    CurrentT = FMath::Fmod(CurrentT + InDeltaTime, CycleTime);
    // 前半往上（0→1），後半往下（1→0）
    const float NormT = CurrentT / (CycleTime * 0.5f);
    const float Pct   = NormT <= 1.f ? NormT : 2.f - NormT;
    UpdatePointerPosition(Pct);
}

void UPhysicalThrowWidget::UpdatePointerPosition(float Pct)
{
    // Pct=0 → 底部，Pct=1 → 頂部；UE5 Y 向下，所以 Pct 大 → Y 小（頂部）
    if (UCanvasPanelSlot* PtrSlot = Pointer ? Cast<UCanvasPanelSlot>(Pointer->Slot) : nullptr)
    {
        const float YOffset = -(BarH * 0.5f) + BarH * (1.f - Pct) - 3.f;
        PtrSlot->SetPosition(FVector2D(-BarW - 23.f, YOffset));
    }
    // 更新填充高度
    if (UCanvasPanelSlot* FillSlot = BarFill ? Cast<UCanvasPanelSlot>(BarFill->Slot) : nullptr)
    {
        FillSlot->SetSize(FVector2D(BarW, BarH * Pct));
        const float FillY = -(BarH * 0.5f) + BarH * (1.f - Pct);
        FillSlot->SetPosition(FVector2D(-BarW - 20.f, FillY));
    }
}

void UPhysicalThrowWidget::StartCharging()
{
    bCharging = true;
    CurrentT  = 0.f;
    SetVisibility(ESlateVisibility::Visible);
}

void UPhysicalThrowWidget::StopCharging()
{
    if (!bCharging) return;
    const float NormT = CurrentT / (CycleTime * 0.5f);
    LockedPct = FMath::Clamp(NormT <= 1.f ? NormT : 2.f - NormT, 0.f, 1.f);
    bCharging = false;
    UpdatePointerPosition(LockedPct);
}

float UPhysicalThrowWidget::GetPowerPct() const
{
    return bCharging ? 0.f : LockedPct;
}

float UPhysicalThrowWidget::GetCurrentChargePct() const
{
    if (bCharging)
    {
        const float NormT = CurrentT / (CycleTime * 0.5f);
        return FMath::Clamp(NormT <= 1.f ? NormT : 2.f - NormT, 0.f, 1.f);
    }
    return LockedPct;
}

void UPhysicalThrowWidget::Reset()
{
    bCharging = false;
    CurrentT  = 0.f;
    LockedPct = 0.f;
    SetVisibility(ESlateVisibility::Collapsed);
}
