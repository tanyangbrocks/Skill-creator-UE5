#pragma once
#include "CoreMinimal.h"
#include "TerrainFeature.h"

// 地表水池特徵（對應 Godot SurfaceWaterPool.cs）。
// 在地表低窪區雕出碗狀凹陷並填入水，形成湖泊地形。
class VOXELWORLD_API FSurfaceWaterPool : public FTerrainFeature
{
public:
    // 最大水池數量上限
    int32 MaxPools     = 3;
    // 水池半徑（tile）
    int32 PoolRadius   = 4;
    // 水池最大深度（tile）
    int32 MaxDepth     = 3;

    virtual FString Name() const override { return TEXT("SurfaceWaterPool"); }

    virtual void Initialize(int32 Seed, int32 WorldWidth, int32 WorldHeight, int32 WorldDepth) override;
    virtual void Prepare(const FTileWorld3D& World, FIntVector ChunkMin, FIntVector ChunkMax) override;
    virtual void PlaceInWorld(FTileWorld3D& World, FIntVector ChunkMin, FIntVector ChunkMax) override;

    virtual EMaterialType GetSurfaceOverride(int32 x, int32 z) const override;

private:
    struct FPoolDesc
    {
        int32 CX = 0;   // 中心 X
        int32 CZ = 0;   // 中心 Z
        int32 SurfaceY = 0;  // 地表 Y（已計算）
    };

    TArray<FPoolDesc> Pools;
    int32             WorldW  = 0;
    int32             WorldH  = 64;
    int32             WorldD  = 0;
    FRandomStream     Rng;
};
