#include "AFloatingDamageActor.h"
#include "UFloatingDamageWidget.h"
#include "Components/WidgetComponent.h"

AFloatingDamageActor::AFloatingDamageActor()
{
    PrimaryActorTick.bCanEverTick = true;

    WidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComp"));
    SetRootComponent(WidgetComp);

    WidgetComp->SetWidgetClass(UFloatingDamageWidget::StaticClass());
    WidgetComp->SetWidgetSpace(EWidgetSpace::World);
    WidgetComp->SetDrawSize(FVector2D(120.f, 60.f));
    WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WidgetComp->SetCastShadow(false);
    // 始終朝向鏡頭
    WidgetComp->SetTwoSided(true);
}

AFloatingDamageActor* AFloatingDamageActor::Spawn(UWorld* World, FVector Location,
                                                   float Amount, bool bIsCrit,
                                                   FLinearColor Color)
{
    if (!World) return nullptr;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AFloatingDamageActor* Actor = World->SpawnActor<AFloatingDamageActor>(
        AFloatingDamageActor::StaticClass(), Location, FRotator::ZeroRotator, Params);
    if (!Actor) return nullptr;

    if (UFloatingDamageWidget* W = Cast<UFloatingDamageWidget>(Actor->WidgetComp->GetWidget()))
    {
        W->SetDamageText(Amount, bIsCrit);
        W->SetDamageColor(Color);
    }
    return Actor;
}

void AFloatingDamageActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    LifeTime += DeltaTime;
    const float T = LifeTime / MaxLifeTime;

    // 向上漂移
    AddActorWorldOffset(FVector(0.f, 0.f, DriftSpeedCm * DeltaTime));

    // 淡出（後半段才開始 fade）
    const float Alpha = FMath::Clamp(1.f - FMath::Max(0.f, (T - 0.5f) * 2.f), 0.f, 1.f);
    if (UFloatingDamageWidget* W = Cast<UFloatingDamageWidget>(WidgetComp->GetWidget()))
        W->SetAlpha(Alpha);

    if (LifeTime >= MaxLifeTime)
        Destroy();
}
