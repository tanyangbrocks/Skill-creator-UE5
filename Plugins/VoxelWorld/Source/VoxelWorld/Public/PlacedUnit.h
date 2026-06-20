#pragma once
#include "CoreMinimal.h"
#include "MaterialType.h"

// 玩家放置的物件單元（對應 Godot PlacedUnit.cs R-6b）。
// 記錄一組由玩家放置的 tile 集合（同一個操作一起放置視為一個 PlacedUnit）。
struct VOXELWORLD_API FPlacedUnit
{
    int32              PlacedUnitId  = 0;                   // 唯一 ID（由 PlacedObjectRegistry 分配）
    EMaterialType      Material      = EMaterialType::Air;  // 放置的材質類型
    TSet<FIntVector>   Tiles;                               // 所有佔用的世界 tile 座標（隨格子被摧毀逐漸縮減）
    float              MaxDurability     = 100.f;
    float              CurrentDurability = 100.f;

    // K-5：放置當下的 tile 總數快照（對應 Godot PlacedUnit.cs Original.Count，註冊後不再變動）。
    // 用於 IsIntact() 判斷「損壞 ≥50% tiles」是否該整體解體（Godot PlacedObjectRegistry.cs:59）。
    int32              OriginalTileCount = 0;

    bool IsIntact() const { return OriginalTileCount <= 0 || Tiles.Num() * 2 >= OriginalTileCount; }

    bool IsValid()     const { return PlacedUnitId > 0 && !Tiles.IsEmpty(); }
    bool IsDestroyed() const { return CurrentDurability <= 0.f; }

    void TakeDamage(float Dmg)
    {
        CurrentDurability = FMath::Max(0.f, CurrentDurability - Dmg);
    }

    bool OccupiesTile(FIntVector Pos) const { return Tiles.Contains(Pos); }
};
