#include "FVoxelInjector.h"
#include "TileWorld3D.h"
#include "WorldScale.h"
#include "GridPos.h"

static void InjectCells(const TArray<FVoxelCell>& Cells,
                        const FTransform& WorldTransform,
                        FTileWorld3D* World, int32 WorldH,
                        FIntVector& OutBoundsMin, FIntVector& OutBoundsMax)
{
    if (!World || Cells.Num() == 0) return;

    OutBoundsMin = FIntVector(INT32_MAX, INT32_MAX, INT32_MAX);
    OutBoundsMax = FIntVector(INT32_MIN, INT32_MIN, INT32_MIN);

    for (const FVoxelCell& Cell : Cells)
    {
        const float TileSize = WorldScale::TileSizeCm;
        const FVector LocalVoxelCm(
            Cell.X * TileSize,
            Cell.Z * TileSize,
            static_cast<float>(WorldH - Cell.Y) * TileSize
        );
        const FVector WorldCm = WorldTransform.TransformPosition(LocalVoxelCm);
        const FGridPos Tile   = WorldScale::WorldToTile(WorldCm, WorldH);

        if (World->GetTile(Tile.X, Tile.Y, Tile.Z) == EMaterialType::Air)
            World->SetTile(Tile.X, Tile.Y, Tile.Z, Cell.Material);

        OutBoundsMin.X = FMath::Min(OutBoundsMin.X, Tile.X);
        OutBoundsMin.Y = FMath::Min(OutBoundsMin.Y, Tile.Y);
        OutBoundsMin.Z = FMath::Min(OutBoundsMin.Z, Tile.Z);
        OutBoundsMax.X = FMath::Max(OutBoundsMax.X, Tile.X);
        OutBoundsMax.Y = FMath::Max(OutBoundsMax.Y, Tile.Y);
        OutBoundsMax.Z = FMath::Max(OutBoundsMax.Z, Tile.Z);
    }
}

void FVoxelInjector::Inject(const TArray<FVoxelCell>& Cells,
                             const FTransform& WorldTransform,
                             FTileWorld3D* World, int32 WorldH,
                             FIntVector& OutBoundsMin, FIntVector& OutBoundsMax)
{
    InjectCells(Cells, WorldTransform, World, WorldH, OutBoundsMin, OutBoundsMax);
}

void FVoxelInjector::Inject(const UVoxelAsset* Asset,
                             const FTransform& WorldTransform,
                             FTileWorld3D* World, int32 WorldH,
                             FIntVector& OutBoundsMin, FIntVector& OutBoundsMax)
{
    if (!Asset) return;
    InjectCells(Asset->Cells, WorldTransform, World, WorldH, OutBoundsMin, OutBoundsMax);
}
