#include "ADroppedItemActor.h"
#include "Components/StaticMeshComponent.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "MaterialType.h"
#include "WorldScale.h"

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
    {
        Destroy();
        return;
    }

    // J-12：重力落下邏輯（Godot DroppedItem.cs:13-35，FallInterval=0.18f）
    FallTimer += DeltaTime;
    constexpr float FallInterval = 0.18f;
    if (FallTimer >= FallInterval)
    {
        FallTimer -= FallInterval;
        if (CachedVoxelWorld)
        {
            if (FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld())
            {
                const FVector Loc   = GetActorLocation();
                const float TileSize = WorldScale::TileSizeCm;
                // 轉換成格座標：Y+ = 向下（UE5 Z+ 向上，但 Tile Y = 向下）
                const int32 tx = FMath::RoundToInt(Loc.X / TileSize);
                const int32 ty = FMath::RoundToInt(Loc.Z / TileSize) + 1;  // 正下方格
                const int32 tz = FMath::RoundToInt(Loc.Y / TileSize);
                if (TW->GetTile(tx, ty, tz) == EMaterialType::Air)
                {
                    SetActorLocation(FVector(Loc.X, Loc.Y, Loc.Z - TileSize));
                }
            }
        }
    }
}
