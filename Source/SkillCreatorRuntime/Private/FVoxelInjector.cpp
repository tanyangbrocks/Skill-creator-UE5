#include "FVoxelInjector.h"
#include "TileWorld3D.h"
#include "WorldScale.h"
#include "GridPos.h"

void FVoxelInjector::Inject(const UVoxelAsset*  Asset,
                             const FTransform&   WorldTransform,
                             FTileWorld3D*       World,
                             int32               WorldH,
                             FIntVector&         OutBoundsMin,
                             FIntVector&         OutBoundsMax)
{
    if (!Asset || !World || Asset->Cells.Num() == 0) return;

    OutBoundsMin = FIntVector(INT32_MAX, INT32_MAX, INT32_MAX);
    OutBoundsMax = FIntVector(INT32_MIN, INT32_MIN, INT32_MIN);

    for (const FVoxelCell& Cell : Asset->Cells)
    {
        // 1. Local tile → Local world cm（voxel 軸 = UE5 軸，此時仍是 voxel local space）
        //    FVoxelCell.Y 向下對應 Voxel-Y，轉到 UE5 時需要翻轉到 Z
        const float TileSize = WorldScale::TileSizeCm;
        const FVector LocalVoxelCm(
            Cell.X * TileSize,
            Cell.Z * TileSize,   // Voxel-Z（南）→ UE5-Y
            static_cast<float>(WorldH - Cell.Y) * TileSize  // Voxel-Y（下）→ UE5-Z（翻轉）
        );

        // 2. 套用物件 Transform（旋轉 / 縮放 / 位移）
        const FVector WorldCm = WorldTransform.TransformPosition(LocalVoxelCm);

        // 3. World cm → voxel tile 座標
        const FGridPos Tile = WorldScale::WorldToTile(WorldCm, WorldH);

        // 4. 只在空格填入（不覆蓋已有地形）
        if (World->GetTile(Tile.X, Tile.Y, Tile.Z) == EMaterialType::Air)
            World->SetTile(Tile.X, Tile.Y, Tile.Z, Cell.Material);

        // 追蹤包圍盒
        OutBoundsMin.X = FMath::Min(OutBoundsMin.X, Tile.X);
        OutBoundsMin.Y = FMath::Min(OutBoundsMin.Y, Tile.Y);
        OutBoundsMin.Z = FMath::Min(OutBoundsMin.Z, Tile.Z);
        OutBoundsMax.X = FMath::Max(OutBoundsMax.X, Tile.X);
        OutBoundsMax.Y = FMath::Max(OutBoundsMax.Y, Tile.Y);
        OutBoundsMax.Z = FMath::Max(OutBoundsMax.Z, Tile.Z);
    }
}
