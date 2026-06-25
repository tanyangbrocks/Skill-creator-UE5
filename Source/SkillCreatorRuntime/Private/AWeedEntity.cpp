#include "AWeedEntity.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "MaterialType.h"
#include "UDroppedItemManager.h"
#include "ItemId.h"
#include "GridPos.h"
#include "WorldScale.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

AWeedEntity::AWeedEntity()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = MeshComp;
    MeshComp->SetCollisionProfileName(TEXT("OverlapAll"));

    // 佔位網格：引擎內建 Plane（100×100 cm）；縮放成 0.4 tile 大小的矮草形狀
    // 未來有專用草地 Mesh 時，直接替換此路徑或在 BP_WeedEntity 覆寫 MeshComp mesh
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
        TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (PlaneFinder.Succeeded())
    {
        MeshComp->SetStaticMesh(PlaneFinder.Object);
        // Plane 預設 100cm 見方；縮放到 0.4 tile × 0.4 tile（雜草約半格寬）
        const float TileSize = WorldScale::TileSizeCm;
        const float S = TileSize * 0.4f / 100.f;
        // Plane 預設朝 +Z；旋轉 90° 使其立起（朝 +X 方向）讓草看起來是豎立的
        MeshComp->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
        MeshComp->SetRelativeScale3D(FVector(S, S, S));
        // 草根對齊地表（tile 頂面）：Plane 旋轉後中心在地表上方 TileSize*0.4*0.5 處
        MeshComp->SetRelativeLocation(FVector(0.f, 0.f, TileSize * 0.2f));
    }
}

void AWeedEntity::BeginPlay()
{
    Super::BeginPlay();

    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVoxelWorldActor::StaticClass(), Found);
    if (Found.Num() > 0)
        CachedVoxelWorld = Cast<AVoxelWorldActor>(Found[0]);
}

void AWeedEntity::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 若下方 tile 不再是 Grass（被挖走、被覆蓋），雜草自毀
    AVoxelWorldActor* VW = CachedVoxelWorld.Get();
    if (!VW) return;
    if (FTileWorld3D* TW = VW->GetTileWorld())
    {
        // TilePos 是雜草生長在其上的 Grass tile（雜草站在 tile 正上方 = TilePos.Y - 1）
        if (TW->GetTile(TilePos.X, TilePos.Y, TilePos.Z) != EMaterialType::Grass)
            Destroy();
    }
}

void AWeedEntity::Collect(APlayerController* Collector)
{
    // 掉落 1 根 Weed 物品（使用 tile 座標，TilePos 已由生成方設定）
    if (UWorld* W = GetWorld())
    {
        if (UDroppedItemManager* Mgr = W->GetSubsystem<UDroppedItemManager>())
        {
            Mgr->SpawnDrop(EItemId::Weed, 1, FGridPos(TilePos.X, TilePos.Y, TilePos.Z));
        }
    }
    Destroy();
}
