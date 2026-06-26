#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ManaSlot.h"
#include "UHpMpCircleWidget.generated.h"

// HP 圓形水缸 + MP 外環（NativePaint 程式化繪製，不需 WBP）
// HP：水位從底部往上填充（橫切弦掃描線）；MP：外環等分弧段，逆時針耗盡。
UCLASS(BlueprintType, Blueprintable)
class SKILLCREATORRUNTIME_API UHpMpCircleWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void UpdateHp(float HpPct, float MpPct);
    void UpdateManaSlots(const TArray<FManaSlot>& Slots);

protected:
    virtual void NativeOnInitialized() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    float             HpPercent  = 1.f;
    float             MpPercent  = 1.f;
    TArray<FManaSlot> ManaSlots;

    static FLinearColor SegColor(int32 Idx, const TArray<FManaSlot>& Slots);

    static void DrawArcSeg(FSlateWindowElementList& Out, int32 Layer,
        const FGeometry& Geo, FVector2D Center, float MidR, float Thickness,
        float A0, float A1, FLinearColor Col);
};
