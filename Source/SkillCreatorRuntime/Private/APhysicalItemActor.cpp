#include "APhysicalItemActor.h"
#include "AVoxelWorldActor.h"
#include "MaterialRegistry.h"
#include "ItemRegistry.h"
#include "WorldScale.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/Class.h"

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

    // ── 展示 Mesh 三層 fallback ─────────────────────────────────────────
    // 1. FItemData::MeshPath（明確設定，最優先）
    // 2. 慣例路徑 /Game/Items/SM_{EnumName}（generate_placeholders.py 產生，可被真實 FBX 覆蓋）
    // 3. Engine BasicShape（Cube/Cylinder/Sphere，依物品分類，確保絕對不空）
    const FItemData& Data = FItemRegistry::Get(InItemId);
    UStaticMesh* Mesh = nullptr;
    bool bNeedBasicShapeScale = false;

    if (!Data.MeshPath.IsNull())
        Mesh = Cast<UStaticMesh>(Data.MeshPath.TryLoad());

    if (!Mesh)
    {
        if (const UEnum* E = StaticEnum<EItemId>())
        {
            const FString Name = E->GetNameStringByValue((int64)InItemId);
            const FString Path = FString::Printf(TEXT("/Game/Items/SM_%s.SM_%s"), *Name, *Name);
            Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
        }
    }

    if (!Mesh)
    {
        // BasicShape 尺寸 100cm；縮到約 1.5 tile（9.4cm @Grain=16）使視覺比例合理
        const TCHAR* FallbackPath = TEXT("/Engine/BasicShapes/Cube.Cube");
        if (Data.bIsConsumable)
            FallbackPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
        else if (Data.bIsWeapon || Data.bIsTool)
            FallbackPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

        Mesh = LoadObject<UStaticMesh>(nullptr, FallbackPath);
        bNeedBasicShapeScale = (Mesh != nullptr);
    }

    // 縮放必須在 SetStaticMesh 前決定，確保 Init() 重複呼叫時不殘留舊縮放
    MeshComp->SetRelativeScale3D(bNeedBasicShapeScale
        ? FVector(WorldScale::TileSizeCm * 1.5f / 100.f)
        : FVector(1.f));

    if (Mesh)
        MeshComp->SetStaticMesh(Mesh);
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
    bLanded       = false;  // 放下後重新受重力，由 PhysicsTick 決定何時落地
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
    if (Cell.MaterialID == 0) return false;  // Air
    // 液體格可穿過（浮力由 PhysicsTick 在 else 分支處理）
    return FMaterialRegistry::GetPhysics(Cell.MaterialID) != EPhysicsCategory::Liquid;
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
