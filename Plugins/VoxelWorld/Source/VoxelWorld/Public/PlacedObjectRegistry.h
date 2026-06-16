#pragma once
#include "CoreMinimal.h"
#include "PlacedUnit.h"

// 玩家放置物件登記系統（對應 Godot PlacedObjectRegistry.cs R-6c）。
// 追蹤所有玩家主動放置的 tile 群組（FPlacedUnit），
// 支援 tile 位置 → PlacedUnit 的快速查找與 JSON 序列化。
class VOXELWORLD_API FPlacedObjectRegistry
{
public:
    // 登記一個新 PlacedUnit（自動分配 PlacedUnitId）；回傳分配到的 ID
    int32 Register(FPlacedUnit& InUnit);

    // 根據 ID 移除（物件被摧毀時呼叫）
    void Unregister(int32 PlacedUnitId);

    // 根據 ID 查找
    FPlacedUnit*       Find(int32 PlacedUnitId);
    const FPlacedUnit* Find(int32 PlacedUnitId) const;

    // 根據 tile 座標查找所在的 PlacedUnit（如無則回傳 nullptr）
    FPlacedUnit*       FindAtTile(FIntVector WorldTile);
    const FPlacedUnit* FindAtTile(FIntVector WorldTile) const;

    // 是否有任何已登記的 PlacedUnit 佔用此 tile
    bool               OccupiedByPlaced(FIntVector WorldTile) const;

    const TArray<FPlacedUnit>& GetAll() const { return Units; }
    int32                      Count()  const { return Units.Num(); }

    void Clear();

    // ── JSON 序列化（供 FlowSaveSystem 存讀 placed-registry.json）────
    FString SerializeToJson() const;
    bool    DeserializeFromJson(const FString& Json);

private:
    TArray<FPlacedUnit>      Units;
    TMap<FIntVector, int32>  TileToUnitId;   // tile → PlacedUnitId 快速查找
    int32                    NextId = 1;

    void RebuildTileIndex();
};
