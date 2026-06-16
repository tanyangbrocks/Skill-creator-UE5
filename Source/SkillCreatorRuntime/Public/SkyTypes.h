#pragma once
#include "CoreMinimal.h"
#include "SkyTypes.generated.h"

// 天空視覺配置（對應 Godot SkyConfig.cs）
USTRUCT(BlueprintType)
struct FSkyConfig
{
    GENERATED_BODY()

    // 天頂顏色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky")
    FLinearColor TopColor      = FLinearColor(0.05f, 0.15f, 0.35f, 1.f);

    // 地平線顏色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky")
    FLinearColor HorizonColor  = FLinearColor(0.55f, 0.65f, 0.75f, 1.f);

    // 雲層覆蓋率（0=晴空 / 1=全覆蓋）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky", meta=(ClampMin=0, ClampMax=1))
    float CloudCoverage = 0.3f;

    // 雲移速度（世界單位 / 秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky")
    float CloudSpeed = 200.f;

    // 雲層顏色
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky")
    FLinearColor CloudColor = FLinearColor(0.85f, 0.88f, 0.92f, 1.f);

    // 整體亮度倍率（影響太陽光強度）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sky", meta=(ClampMin=0))
    float Brightness = 1.f;
};
