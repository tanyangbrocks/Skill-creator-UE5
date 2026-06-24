#include "AWeedEntity.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "MaterialType.h"
#include "UDroppedItemManager.h"
#include "ItemId.h"
#include "GridPos.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

AWeedEntity::AWeedEntity()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = MeshComp;
    // 網格由 Content Browser 的 BP_WeedEntity 設定；C++ 只建立 slot
    MeshComp->SetCollisionProfileName(TEXT("OverlapAll"));
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
