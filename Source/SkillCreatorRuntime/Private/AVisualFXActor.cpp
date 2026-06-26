#include "AVisualFXActor.h"

AVisualFXActor::AVisualFXActor()
{
    PrimaryActorTick.bCanEverTick = false; // 子類依需求覆寫為 true
}

void AVisualFXActor::BeginPlay()
{
    Super::BeginPlay();
    if (MaxLifetime > 0.f)
        GetWorldTimerManager().SetTimer(LifetimeTimer, this, &AVisualFXActor::OnLifetimeExpired, MaxLifetime, false);
}

void AVisualFXActor::OnLifetimeExpired()
{
    Destroy();
}
