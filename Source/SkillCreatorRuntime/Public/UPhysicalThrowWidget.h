#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UPhysicalThrowWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UCanvasPanelSlot;

// 投擲力量條 UI（G-7）
// 右側縱向長條，指針從底部到頂部循環 CycleTime 秒；停止時鎖定當前位置。
UCLASS()
class SKILLCREATORRUNTIME_API UPhysicalThrowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void StartCharging();
    void StopCharging();
    float GetPowerPct() const;
    void  Reset();

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    static constexpr float BarH      = 200.f;
    static constexpr float BarW      = 20.f;
    static constexpr float CycleTime = 3.f;    // 一來一回秒數（可調）

    float CurrentT  = 0.f;
    bool  bCharging = false;
    float LockedPct = 0.f;

    UPROPERTY() TObjectPtr<UCanvasPanel> Root;
    UPROPERTY() TObjectPtr<UBorder>      BarBackground;
    UPROPERTY() TObjectPtr<UBorder>      BarFill;
    UPROPERTY() TObjectPtr<UBorder>      Pointer;

    void UpdatePointerPosition(float Pct);
};
