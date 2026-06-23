#include "ADebrisActor.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ADebrisActor::ADebrisActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
        Mesh->SetStaticMesh(SphereMesh.Object);

    Mesh->SetRelativeScale3D(FVector(0.15f));          // ≈ 15cm 小球
    Mesh->SetSimulatePhysics(true);
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetLinearDamping(0.5f);
    Mesh->SetAngularDamping(1.0f);
}

void ADebrisActor::Init(EItemId InId, int32 InCount)
{
    FragmentItemId = InId;
    FragmentCount  = FMath::Max(0, InCount);
    Age            = 0.f;
}

void ADebrisActor::LaunchImpulse(FVector VelocityCms)
{
    // bVelChange=true：直接設速度，不乘質量（碎塊質量不影響飛行視覺）
    Mesh->AddImpulse(VelocityCms, NAME_None, /*bVelChange=*/true);
}

bool ADebrisActor::CanPickup(FVector PlayerWorldPos) const
{
    return FVector::DistSquared(GetActorLocation(), PlayerWorldPos) <= PickupRadius * PickupRadius;
}

void ADebrisActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    Age += DeltaTime;
    if (Lifetime > 0.f && Age >= Lifetime)
        Destroy();
}
