#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AVisualFXActor.generated.h"

// 純視覺 / 特效 Actor 的共同基底類
// - 不持有任何 Mesh / Widget 元件（各子類自行建立適合的視覺元件）
// - 提供可選的 MaxLifetime 計時器自動 Destroy
//   · MaxLifetime > 0 → BeginPlay 後經過該秒數自動 Destroy
//   · MaxLifetime = 0 → 子類自行控制生命週期（適用需要逐幀動畫的 FX，如 AFloatingDamageActor）
// 子類：AFloatingDamageActor（漂浮傷害數字）
UCLASS(Abstract)
class SKILLCREATORRUNTIME_API AVisualFXActor : public AActor
{
    GENERATED_BODY()
public:
    AVisualFXActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VisualFX")
    float MaxLifetime = 0.f;

protected:
    virtual void BeginPlay() override;

private:
    FTimerHandle LifetimeTimer;
    void OnLifetimeExpired();
};
