#include "APhysicalItemActor.h"
#include "AVoxelWorldActor.h"
#include "MaterialRegistry.h"
#include "ItemRegistry.h"
#include "WorldScale.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

APhysicalItemActor::APhysicalItemActor()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RootComponent = MeshComp;

    // 撿取偵測用球形碰撞（Query Only，不做物理模擬）
    PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
    PickupSphere->SetupAttachment(RootComponent);
    PickupSphere->SetSphereRadius(WorldScale::TileSizeCm * 3.f);
    PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PickupSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
}

void APhysicalItemActor::BeginPlay()
{
    Super::BeginPlay();
    CachedVoxelWorld = AVoxelWorldActor::FindInWorld(GetWorld());
}

void APhysicalItemActor::Init(EItemId InItemId, int32 InCount, FVector InitialVelocityCms)
{
    ItemId   = InItemId;
    Count    = InCount;
    Velocity = InitialVelocityCms;
    bLanded  = false;
}

float APhysicalItemActor::GetMass() const
{
    return FItemRegistry::Get(ItemId).Mass;
}

FText APhysicalItemActor::GetPickupHintText() const
{
    const FText Name = FItemRegistry::Get(ItemId).DisplayName;
    return FText::Format(NSLOCTEXT("PhysicalItem", "PickupHint", "G 拿取 {0}"), Name);
}

void APhysicalItemActor::OnCarried(ASkillCreatorCharacter* InCarrier)
{
    bBeingCarried = true;
    Carrier       = InCarrier;
    Velocity      = FVector::ZeroVector;
    bLanded       = false;
    SetActorEnableCollision(false);
}

void APhysicalItemActor::OnReleased(FVector ThrowVelocityCms)
{
    bBeingCarried = false;
    Carrier       = nullptr;
    Velocity      = ThrowVelocityCms;
    bLanded       = (ThrowVelocityCms.IsNearlyZero());
    SetActorEnableCollision(true);
}

void APhysicalItemActor::Tick(float DeltaTime)
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
        return;
    }

    if (!bLanded)
        PhysicsTick(DeltaTime);
}

void APhysicalItemActor::PhysicsTick(float DeltaTime)
{
    // 重力（UE5 Z=上，重力方向 -Z）
    Velocity.Z -= WorldScale::GlobalGravityScale * 980.f * WorldScale::GravityScaleMult * DeltaTime;

    FVector NewPos = GetActorLocation() + Velocity * DeltaTime;
    FGridPos NewTile = WorldScale::WorldToTile(NewPos);

    // 檢查目標格是否為實體
    if (IsSolidAt(NewTile.X, NewTile.Y, NewTile.Z))
    {
        HandleTileCollision(NewTile.X, NewTile.Y, NewTile.Z);
    }
    else
    {
        // 浮力：腳下是液體材質時施加向上力抵消重力
        FGridPos BelowTile = WorldScale::WorldToTile(NewPos - FVector(0.f, 0.f, WorldScale::TileSizeCm * 0.5f));
        if (CachedVoxelWorld.IsValid())
        {
            FTileCell BelowCell = CachedVoxelWorld->GetTileWorld()->GetCell(BelowTile.X, BelowTile.Y, BelowTile.Z);
            EPhysicsCategory PhysCat = FMaterialRegistry::GetPhysics(BelowCell.MaterialID);
            if (PhysCat == EPhysicsCategory::Liquid)
                Velocity.Z += WorldScale::GlobalGravityScale * 980.f * WorldScale::GravityScaleMult * DeltaTime * 1.5f;
        }
        SetActorLocation(NewPos);
    }
}

void APhysicalItemActor::HandleTileCollision(int32 VoxX, int32 VoxY, int32 VoxZ)
{
    // 取腳下 tile 的 Restitution
    float R = GetBelowRestitution();
    Velocity.Z = -Velocity.Z * R;
    Velocity.X *= 0.8f;
    Velocity.Y *= 0.8f;
    if (FMath::Abs(Velocity.Z) < 5.f)
    {
        Velocity = FVector::ZeroVector;
        bLanded  = true;
    }
}

bool APhysicalItemActor::IsSolidAt(int32 VoxX, int32 VoxY, int32 VoxZ) const
{
    if (!CachedVoxelWorld.IsValid()) return false;
    FTileCell Cell = CachedVoxelWorld->GetTileWorld()->GetCell(VoxX, VoxY, VoxZ);
    return Cell.MaterialID != 0;  // 0 = Air
}

float APhysicalItemActor::GetBelowRestitution() const
{
    if (!CachedVoxelWorld.IsValid()) return 0.f;
    FVector BelowPos = GetActorLocation() - FVector(0.f, 0.f, WorldScale::TileSizeCm * 0.6f);
    FGridPos BelowTile = WorldScale::WorldToTile(BelowPos);
    FTileCell Cell = CachedVoxelWorld->GetTileWorld()->GetCell(BelowTile.X, BelowTile.Y, BelowTile.Z);
    EMaterialType Mat = static_cast<EMaterialType>(Cell.MaterialID);
    return FMaterialRegistry::Get(Mat).Restitution;
}
