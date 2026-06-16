#pragma once
#include "CoreMinimal.h"
#include "MaterialType.h"

class FTileWorld3D;
class FPlacedObjectRegistry;

// 放置合法性驗證器（對應 Godot PlacementValidator.cs R-6d）。
// 靜態工具類別：CanPlace 判斷指定 tile 集合是否可合法放置。
struct VOXELWORLD_API FPlacementValidator
{
    // 是否可放置：
    //   - 目標 tiles 都在世界範圍內
    //   - 目標 tiles 都是 Air（空格）
    //   - 目標 tiles 沒有被其他 PlacedUnit 佔用（Registry 可為 nullptr = 忽略此檢查）
    static bool CanPlace(const FTileWorld3D& World,
                         const TArray<FIntVector>& Tiles,
                         EMaterialType Material,
                         const FPlacedObjectRegistry* Registry = nullptr);
};
