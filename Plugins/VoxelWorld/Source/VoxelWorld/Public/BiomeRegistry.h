#pragma once
#include "CoreMinimal.h"
#include "MaterialType.h"

// 群系定義（溫度/濕度定義範圍，地表/次表材質，樹木密度）
struct VOXELWORLD_API FBiomeDef
{
    FName          BiomeId;
    float          TempMin, TempMax;     // 溫度範圍 -1..1
    float          HumidMin, HumidMax;   // 濕度範圍 -1..1
    EMaterialType  SurfaceMat    = EMaterialType::Grass;
    EMaterialType  SubsurfaceMat = EMaterialType::Dirt_Dry;
    float          TreeDensityMult = 1.0f;  // 乘上基礎 20% 種樹機率
};

// 純靜態登錄表（thread-safe 讀取：Initialize() 在世界初始化完成，此後只有 Query()）
// 新增群系 SOP：在 BiomeRegistry.cpp 的 Initialize() 加一行 Biomes.Add({...}) → Live Coding Build
class VOXELWORLD_API FBiomeRegistry
{
public:
    static void Initialize();
    static const FBiomeDef& Query(float Temperature, float Humidity);
    static const FBiomeDef& GetDefault();  // Temperate fallback

private:
    static TArray<FBiomeDef> Biomes;
    static bool bInitialized;
};
