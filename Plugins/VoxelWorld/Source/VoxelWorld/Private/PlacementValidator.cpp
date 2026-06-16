#include "PlacementValidator.h"
#include "TileWorld3D.h"
#include "PlacedObjectRegistry.h"

bool FPlacementValidator::CanPlace(const FTileWorld3D& World,
                                   const TArray<FIntVector>& Tiles,
                                   EMaterialType Material,
                                   const FPlacedObjectRegistry* Registry)
{
    if (Tiles.IsEmpty() || Material == EMaterialType::Air) return false;

    for (const FIntVector& T : Tiles)
    {
        // 在世界邊界內
        if (!World.InBounds(T.X, T.Y, T.Z)) return false;

        // 目標位置必須是 Air
        if (World.GetTile(T.X, T.Y, T.Z) != EMaterialType::Air) return false;

        // 不能與已登記的 PlacedUnit 重疊
        if (Registry && Registry->OccupiedByPlaced(T)) return false;
    }
    return true;
}
