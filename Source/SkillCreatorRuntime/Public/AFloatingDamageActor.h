#pragma once
#include "CoreMinimal.h"
#include "AVisualFXActor.h"
#include "AFloatingDamageActor.generated.h"

class UWidgetComponent;
class UFloatingDamageWidget;

// 浮動傷害數字 Actor（對應 Godot FloatingDamageNumber.cs）。
// 生成後向上漂移並淡出，自動 Destroy。
// MaxLifetime = 0（不用基底計時器）；自有 LifeTime 兼作淡出進度 T 的時間戳。
UCLASS()
class SKILLCREATORRUNTIME_API AFloatingDamageActor : public AVisualFXActor
{
    GENERATED_BODY()
public:
    AFloatingDamageActor();

    // 工廠方法：在指定世界位置生成浮動傷害數字
    static AFloatingDamageActor* Spawn(UWorld* World, FVector Location,
                                       float Amount, bool bIsCrit = false,
                                       FLinearColor Color = FLinearColor::White);

    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY() TObjectPtr<UWidgetComponent> WidgetComp;

    float LifeTime      = 0.f;
    static constexpr float MaxLifeTime  = 1.2f;   // 秒：數字存活總時長
    static constexpr float DriftSpeedCm = 40.f;   // 每秒向上漂移（cm）
};
