#include "ASkyController.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"

ASkyController::ASkyController()
{
    PrimaryActorTick.bCanEverTick = true;

    SunLight   = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
    Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>   (TEXT("Atmosphere"));
    SkyLight   = CreateDefaultSubobject<USkyLightComponent>        (TEXT("SkyLight"));

    SetRootComponent(SunLight);
    Atmosphere->SetupAttachment(SunLight);
    SkyLight->SetupAttachment(SunLight);

    // RealTime sky light capture so it responds to atmosphere changes
    SkyLight->SetMobility(EComponentMobility::Movable);
}

void ASkyController::BeginPlay()
{
    Super::BeginPlay();
    PushConfig();
}

void ASkyController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // 雲移動
    CloudOffset += Config.CloudSpeed * DeltaSeconds;

    if (TransitionRemaining > 0.f)
    {
        TransitionRemaining -= DeltaSeconds;
        float Alpha = 1.f - FMath::Max(0.f, TransitionRemaining) / TransitionDuration;
        Alpha = FMath::Clamp(Alpha, 0.f, 1.f);

        FSkyConfig Lerped;
        Lerped.TopColor      = FLinearColor::LerpUsingHSV(TransitionFrom.TopColor,     TransitionTarget.TopColor,     Alpha);
        Lerped.HorizonColor  = FLinearColor::LerpUsingHSV(TransitionFrom.HorizonColor, TransitionTarget.HorizonColor, Alpha);
        Lerped.CloudColor    = FLinearColor::LerpUsingHSV(TransitionFrom.CloudColor,   TransitionTarget.CloudColor,   Alpha);
        Lerped.CloudCoverage = FMath::Lerp(TransitionFrom.CloudCoverage, TransitionTarget.CloudCoverage, Alpha);
        Lerped.CloudSpeed    = FMath::Lerp(TransitionFrom.CloudSpeed,    TransitionTarget.CloudSpeed,    Alpha);
        Lerped.Brightness    = FMath::Lerp(TransitionFrom.Brightness,    TransitionTarget.Brightness,    Alpha);

        if (TransitionRemaining <= 0.f)
        {
            Config = TransitionTarget;
            TransitionRemaining = 0.f;
        }

        ApplyToComponents(Lerped);
        OnSkyConfigChanged(Lerped);
    }
}

void ASkyController::PushConfig()
{
    ApplyToComponents(Config);
    OnSkyConfigChanged(Config);
}

void ASkyController::ApplyConfig(const FSkyConfig& NewConfig)
{
    Config = NewConfig;
    PushConfig();
}

void ASkyController::TransitionTo(const FSkyConfig& Target, float DurationSec)
{
    TransitionFrom      = Config;
    TransitionTarget    = Target;
    TransitionDuration  = FMath::Max(DurationSec, KINDA_SMALL_NUMBER);
    TransitionRemaining = TransitionDuration;
}

void ASkyController::ApplyToComponents(const FSkyConfig& C)
{
    if (SunLight)
    {
        // 基礎強度 75000 lux（晴天），乘以 Brightness 倍率
        SunLight->SetIntensity(75000.f * C.Brightness);
        SunLight->SetLightColor(C.TopColor.ToFColor(true));
    }

    if (Atmosphere)
    {
        // 天頂 / 地平線顏色映射到 Rayleigh 散射
        const float RayleighScale = FMath::Lerp(0.5f, 2.f, 1.f - C.TopColor.GetLuminance());
        Atmosphere->SetRayleighScatteringScale(RayleighScale);

        // Mie 散射控制地平線霧感
        const float MieScale = FMath::Lerp(0.1f, 3.f, C.HorizonColor.GetLuminance());
        Atmosphere->SetMieScatteringScale(MieScale);
    }
}
