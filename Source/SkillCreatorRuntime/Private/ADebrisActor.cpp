#include "ADebrisActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "WorldScale.h"
#include "ItemRegistry.h"

ADebrisActor::ADebrisActor()
{
    PrimaryActorTick.bCanEverTick = true;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
        Mesh->SetStaticMesh(SphereMesh.Object);

    Mesh->SetRelativeScale3D(FVector(0.15f));
    Mesh->SetSimulatePhysics(true);
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
    Mesh->SetLinearDamping(0.5f);
    Mesh->SetAngularDamping(1.0f);

    // 撿取偵測球（G-10）
    PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
    PickupSphere->SetupAttachment(RootComponent);
    PickupSphere->SetSphereRadius(WorldScale::TileSizeCm * 3.f);
    PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PickupSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
}

void ADebrisActor::BeginPlay()
{
    Super::BeginPlay();
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

    if (bBeingCarried)
    {
        // 跟隨攜帶者，懸浮在角色前方
        if (Carrier.IsValid())
        {
            FVector HoldPos = Carrier->GetActorLocation()
                + Carrier->GetActorForwardVector() * WorldScale::TileSizeCm * 2.f
                + FVector(0.f, 0.f, WorldScale::TileSizeCm * 1.f);
            SetActorLocation(HoldPos);
        }
        return;  // 攜帶中不計 Age，也不自毀
    }

    Age += DeltaTime;
    if (Lifetime > 0.f && Age >= Lifetime)
        Destroy();
}

// ── G-10 IPhysicalPickable 實作 ───────────────────────────────────────────

FText ADebrisActor::GetPickupHintText() const
{
    if (FragmentItemId != EItemId::None)
    {
        const FText Name = FItemRegistry::Get(FragmentItemId).DisplayName;
        return FText::Format(NSLOCTEXT("Debris", "PickupHint", "G 拿取碎塊（{0}）"), Name);
    }
    return NSLOCTEXT("Debris", "PickupHintGeneric", "G 拿取碎塊");
}

void ADebrisActor::OnCarried(ASkillCreatorCharacter* InCarrier)
{
    bBeingCarried = true;
    Carrier       = InCarrier;
    // 停止 Chaos 物理，改由 Tick 手動跟隨
    if (Mesh) Mesh->SetSimulatePhysics(false);
    SetActorEnableCollision(false);
}

void ADebrisActor::OnReleased(FVector ThrowVelocityCms)
{
    bBeingCarried = false;
    Carrier       = nullptr;
    SetActorEnableCollision(true);
    // 恢復 Chaos 物理並給予投擲初速
    if (Mesh)
    {
        Mesh->SetSimulatePhysics(true);
        if (!ThrowVelocityCms.IsNearlyZero())
            Mesh->AddImpulse(ThrowVelocityCms * GetMass(), NAME_None, /*bVelChange=*/false);
    }
}
