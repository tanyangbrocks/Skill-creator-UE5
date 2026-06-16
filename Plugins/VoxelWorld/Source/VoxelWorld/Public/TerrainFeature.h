#pragma once
#include "CoreMinimal.h"
#include "MaterialType.h"

class FTileWorld3D;

// 地形後置特徵抽象基類（對應 Godot TerrainFeature.cs）。
// 每個子類代表一種地形後置處理（水池 / 峽谷 / 遺跡等）。
// MapGenerator3D::PostProcessRegion 呼叫 PlaceInWorld 完成地形修飾。
class VOXELWORLD_API FTerrainFeature
{
public:
    virtual ~FTerrainFeature() = default;

    virtual FString Name() const = 0;

    // 初始化（傳入 WorldSeed + 地形參數）
    virtual void Initialize(int32 Seed, int32 WorldWidth, int32 WorldHeight, int32 WorldDepth) = 0;

    // 預備（在 chunk 全部 Apply 後、PlaceInWorld 前呼叫）
    virtual void Prepare(const FTileWorld3D& World,
                         FIntVector ChunkMin, FIntVector ChunkMax) = 0;

    // 實際放置（修改 World tile）
    virtual void PlaceInWorld(FTileWorld3D& World,
                              FIntVector ChunkMin, FIntVector ChunkMax) = 0;

    // 回傳指定 (x,z) 位置的地表材質覆蓋（EMaterialType::Air = 不覆蓋）
    virtual EMaterialType GetSurfaceOverride(int32 x, int32 z) const
    {
        return EMaterialType::Air;
    }
};
