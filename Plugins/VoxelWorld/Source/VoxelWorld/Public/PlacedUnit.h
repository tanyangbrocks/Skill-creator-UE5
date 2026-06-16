#pragma once
#include "CoreMinimal.h"
#include "MaterialType.h"

// 玩家放置的物件單元（對應 Godot PlacedUnit.cs R-6b）。
// 記錄一組由玩家放置的 tile 集合（同一個操作一起放置視為一個 PlacedUnit）。
struct VOXELWORLD_API FPlacedUnit
{
    int32              PlacedUnitId  = 0;                   // 唯一 ID（由 PlacedObjectRegistry 分配）
    EMaterialType      Material      = EMaterialType::Air;  // 放置的材質類型
    TSet<FIntVector>   Tiles;                               // 所有佔用的世界 tile 座標
    float              MaxDurability     = 100.f;
    float              CurrentDurability = 100.f;

    bool IsValid()     const { return PlacedUnitId > 0 && !Tiles.IsEmpty(); }
    bool IsDestroyed() const { return CurrentDurability <= 0.f; }

    void TakeDamage(float Dmg)
    {
        CurrentDurability = FMath::Max(0.f, CurrentDurability - Dmg);
    }

    bool OccupiesTile(FIntVector Pos) const { return Tiles.Contains(Pos); }
};
