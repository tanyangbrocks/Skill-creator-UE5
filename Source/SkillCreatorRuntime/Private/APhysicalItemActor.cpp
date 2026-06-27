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

    // PHYS-3: Chaos Physics 碰撞 + 模擬（取代舊 NoCollision + 手動 PhysicsTick）
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComp->SetCollisionObjectType(ECC_PhysicsBody);
    MeshComp->SetCollisionResponseToAllChannels(ECR_Block);
    MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    MeshComp->SetSimulatePhysics(true);
    MeshComp->SetLinearDamping(0.5f);
    MeshComp->SetAngularDamping(1.0f);
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
    ItemId = InItemId;
    Count  = InCount;

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

    // PHYS-3: 初始速度由 Chaos 接管
    if (!InitialVelocityCms.IsZero())
        MeshComp->SetPhysicsLinearVelocity(InitialVelocityCms);
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
    // PHYS-3: 關閉 Chaos 模擬，物品跟隨角色手持位置
    MeshComp->SetSimulatePhysics(false);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetActorEnableCollision(false);
}

void APhysicalItemActor::OnReleased(FVector ThrowVelocityCms)
{
    bBeingCarried = false;
    Carrier       = nullptr;
    // PHYS-3: 重啟 Chaos 模擬，施加投擲速度
    SetActorEnableCollision(true);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComp->SetSimulatePhysics(true);
    if (!ThrowVelocityCms.IsZero())
        MeshComp->SetPhysicsLinearVelocity(ThrowVelocityCms);
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

    // PHYS-4: 液體浮力——查詢當前 tile，若為 Liquid 調高阻尼 + 施加向上力
    if (CachedVoxelWorld.IsValid())
    {
        FGridPos CurTile = WorldScale::WorldToTile(GetActorLocation());
        FTileCell Cell   = CachedVoxelWorld->GetTileWorld()->GetCell(CurTile.X, CurTile.Y, CurTile.Z);
        bool bNowInLiquid = (FMaterialRegistry::GetPhysics(Cell.MaterialID) == EPhysicsCategory::Liquid);

        if (bNowInLiquid != bIsInLiquid)
        {
            bIsInLiquid = bNowInLiquid;
            MeshComp->SetLinearDamping(bIsInLiquid  ? 3.0f : 0.5f);
            MeshComp->SetAngularDamping(bIsInLiquid ? 3.0f : 1.0f);
        }

        if (bIsInLiquid)
        {
            // 向上加速度 = 1.2× 重力 → 淨上浮 0.2g（bAccelChange=true 不乘質量）
            float BuoyancyAccel = -GetWorld()->GetGravityZ() * 1.2f;
            MeshComp->AddForce(FVector(0.f, 0.f, BuoyancyAccel), NAME_None, /*bAccelChange=*/true);
        }
    }
}
