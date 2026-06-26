#include "AWeedEntity.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "MaterialType.h"
#include "UDroppedItemManager.h"
#include "ItemId.h"
#include "GridPos.h"
#include "WorldScale.h"
#include "Components/StaticMeshComponent.h"

AWeedEntity::AWeedEntity()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp->SetCollisionProfileName(TEXT("OverlapAll"));

    // 佔位網格：引擎內建 Plane（100×100 cm）縮放成 0.4 tile 大小的矮草形狀
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(
        TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (PlaneFinder.Succeeded())
    {
        MeshComp->SetStaticMesh(PlaneFinder.Object);
        const float TileSize = WorldScale::TileSizeCm;
        const float S = TileSize * 0.4f / 100.f;
        MeshComp->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
        MeshComp->SetRelativeScale3D(FVector(S, S, S));
        MeshComp->SetRelativeLocation(FVector(0.f, 0.f, TileSize * 0.2f));
    }
}

void AWeedEntity::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 若 AnchorTile 不再是 Grass（被挖走、被覆蓋），雜草自毀
    AVoxelWorldActor* VW = CachedVoxelWorld.Get();
    if (!VW) return;
    if (FTileWorld3D* TW = VW->GetTileWorld())
    {
        if (TW->GetTile(AnchorTile.X, AnchorTile.Y, AnchorTile.Z) != EMaterialType::Grass)
            Destroy();
    }
}

void AWeedEntity::Collect(APlayerController* Collector)
{
    if (UWorld* W = GetWorld())
    {
        if (UDroppedItemManager* Mgr = W->GetSubsystem<UDroppedItemManager>())
            Mgr->SpawnDrop(EItemId::Weed, 1, FGridPos(AnchorTile.X, AnchorTile.Y, AnchorTile.Z));
    }
    Destroy();
}
