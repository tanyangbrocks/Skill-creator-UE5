#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkyTypes.h"
#include "ASkyController.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;

// 天空控制器（對應 Godot SkyController.cs）
// 掌管太陽光強度、大氣散射顏色；雲層材質參數透過 OnSkyConfigChanged 廣播至 Blueprint。
// 關卡放一個此 Actor，或由 GameMode 自動 Spawn。
UCLASS(Blueprintable)
class SKILLCREATORRUNTIME_API ASkyController : public AActor
{
    GENERATED_BODY()
public:
    ASkyController();

    // 目前天空配置（可在 Editor 調整預設值）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky")
    FSkyConfig Config;

    // 套用 Config 到場景元件；同步呼叫 OnSkyConfigChanged
    UFUNCTION(BlueprintCallable, Category="Sky")
    void PushConfig();

    // 使用給定配置覆寫並立即套用
    UFUNCTION(BlueprintCallable, Category="Sky")
    void ApplyConfig(const FSkyConfig& NewConfig);

    // 平滑插值到目標配置（DurationSec 秒內完成）
    UFUNCTION(BlueprintCallable, Category="Sky")
    void TransitionTo(const FSkyConfig& Target, float DurationSec);

    // Blueprint 側實作：材質參數推送（雲層 UMD / 天空球 Material）
    UFUNCTION(BlueprintImplementableEvent, Category="Sky")
    void OnSkyConfigChanged(const FSkyConfig& NewConfig);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDirectionalLightComponent> SunLight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkyAtmosphereComponent>    Atmosphere;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkyLightComponent>         SkyLight;

    // Transition state
    FSkyConfig TransitionFrom;
    FSkyConfig TransitionTarget;
    float      TransitionRemaining = 0.f;
    float      TransitionDuration  = 0.f;

    // 逐幀雲移動偏移（傳給 OnSkyConfigChanged 的 CloudOffset）
    float CloudOffset = 0.f;

    void ApplyToComponents(const FSkyConfig& C);
};
