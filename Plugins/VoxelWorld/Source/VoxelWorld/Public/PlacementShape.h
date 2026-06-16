#pragma once
#include "CoreMinimal.h"

// 玩家放置形狀（對應 Godot PlacementShape.cs R-6a）
enum class EPlacementShape : uint8
{
    Single    = 0,   // 單一 tile
    Cube      = 1,   // N×N×N 立方體
    Sphere    = 2,   // 球形（半徑 R）
    Cylinder  = 3,   // 垂直圓柱（半徑 R，高 H）
    Flat      = 4,   // 平面（N×N，高度 1）
};

struct VOXELWORLD_API FPlacementShape
{
    // 取得指定形狀的所有相對偏移（相對於中心 tile）
    static TArray<FIntVector> GetOffsets(EPlacementShape Shape, int32 Radius);

private:
    static TArray<FIntVector> CubeOffsets(int32 Radius);
    static TArray<FIntVector> SphereOffsets(int32 Radius);
    static TArray<FIntVector> CylinderOffsets(int32 Radius, int32 Height);
    static TArray<FIntVector> FlatOffsets(int32 Radius);
};
