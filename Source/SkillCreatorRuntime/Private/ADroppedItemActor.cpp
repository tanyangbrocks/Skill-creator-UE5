#include "ADroppedItemActor.h"
#include "Components/StaticMeshComponent.h"

ADroppedItemActor::ADroppedItemActor()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = MeshComp;
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADroppedItemActor::Init(EItemId InId, int32 InCount)
{
    ItemId = InId;
    Count  = FMath::Max(1, InCount);
    Age    = 0.f;
}

bool ADroppedItemActor::CanPickup(FVector PlayerWorldPos) const
{
    const float DistSq = FVector::DistSquared(GetActorLocation(), PlayerWorldPos);
    const float R      = PickupRadius * 100.f;   // tile → cm
    return DistSq <= R * R;
}

void ADroppedItemActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    Age += DeltaTime;
    if (IsExpired())
        Destroy();
}
