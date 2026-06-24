#pragma once
#include "CoreMinimal.h"
#include "MaterialType.h"

// 地表水池特徵（對應 Godot Scripts/World/Terrain/SurfaceWaterPool.cs）。
// 在地表隨機散布若干水池（碗型凹陷 + 平靜水面）。UE5 世界生成是純懶加載（沒有 Godot 的
// 「初始條帶 PlaceInWorld + 其餘懶加載 GetSurfaceOverride」雙路徑），所有 chunk 一律經由
// FMapGenerator3D::ComputeChunkData() 生成，因此這裡只保留 Initialize/Prepare/QueryOverride
// 三個方法：Initialize 算佈局，Prepare 固定水面 Y，QueryOverride 給每個 chunk 的每個 tile 查詢。
//
// 座標慣例：UE5 Y 增大 = 向下（對應 Godot 的反方向，Godot Y 增大 = 向上）。
class VOXELWORLD_API FSurfaceWaterPool
{
public:
    struct FPoolDesc
    {
        int32 CX = 0;
        int32 CZ = 0;
        int32 Radius = 0;
        int32 MaxDepth = 0;
        int32 WaterSurfaceY = 0;  // Prepare() 算出
    };

    // 從 seed 產生全地圖水池佈局（位置/半徑/深度）。對應 Godot Initialize()。
    // WorldWidth/WorldDepth <= 0 視為無邊界世界（AVoxelWorldActor 預設無限懶載入），
    // 此時改以 spawn（世界原點）為中心、固定散布半徑取代 Godot 的 EdgeMargin~WorldW-EdgeMargin。
    void Initialize(int32 Seed, int32 WorldWidth, int32 WorldHeight, int32 WorldDepth);

    // 用地表高度函數固定各池水面 Y。對應 Godot Prepare(Func<int,int,int> getHeight)。
    void Prepare(const TFunction<int32(int32, int32)>& GetHeight);

    const TArray<FPoolDesc>& GetPools() const { return Pools; }

    // 懶加載 chunk 生成時，每個 (WorldX,WorldZ) 查詢覆寫。對應 Godot GetSurfaceOverride()。
    // 回傳 true 時 OutEffectiveY/OutMat 有效（OutMat 為 Water 或 Dirt）。
    // 純函數，只讀 Pools（thread-safe，可在背景 thread 對 GetPools() 的拷貝呼叫）。
    static bool QueryOverride(const TArray<FPoolDesc>& Pools, int32 WorldX, int32 WorldZ,
                               int32 NaturalSurfaceY, int32& OutEffectiveY, EMaterialType& OutMat);

private:
    TArray<FPoolDesc> Pools;
    int32 WorldW = 0;
    int32 WorldH = 64;
    int32 WorldD = 0;

    // ── 生成參數（對應 Godot 常數）─────────────────────────────────────────
    static constexpr int32 CountMin   = 4;
    static constexpr int32 CountMax   = 9;    // exclusive（Godot rng.Next 上界不含）
    static constexpr int32 RadiusMin  = 3;    // ×Grain（遊戲單位）
    static constexpr int32 RadiusMax  = 10;   // ×Grain，exclusive
    static constexpr int32 DepthMin   = 10;   // tile
    static constexpr int32 DepthMax   = 50;   // tile，exclusive
    static constexpr int32 EdgeMargin = 32;   // 有限世界時離邊界的最小距離
    static constexpr int32 ScatterRadius = 400; // 無邊界世界時，池中心散布半徑（tile）
    static constexpr float WaterFill  = 0.7f; // 水面在碗深的位置比例：0=碗底，1=碗口（滿水）
};
