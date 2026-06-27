#include "BiomeRegistry.h"

TArray<FBiomeDef> FBiomeRegistry::Biomes;
bool FBiomeRegistry::bInitialized = false;

// ============================================================
// BIO-1：初始群系表（5 種）
// 新增群系 SOP：
//   1. 在此函式加一行 Biomes.Add({...}) 並調整 Temp/Humid 範圍
//   2. Build（.cpp 改，可 Live Coding）
// ============================================================

void FBiomeRegistry::Initialize()
{
    if (bInitialized) return;
    bInitialized = true;
    Biomes.Empty();

    // 溫帶（temperate）：中溫中濕，草地標準地貌，樹木茂密
    Biomes.Add({ "Temperate",
        -0.1f, 0.6f,    // Temp: -0.1 ~ 0.6
         0.2f, 0.8f,    // Humid: 0.2 ~ 0.8
        EMaterialType::Grass, EMaterialType::Dirt_Dry, 1.0f });

    // 沙漠（desert）：高溫低濕，沙地，幾乎無樹木
    Biomes.Add({ "Desert",
         0.5f, 1.0f,    // Temp: 0.5 ~ 1.0
        -1.0f, 0.3f,    // Humid: -1.0 ~ 0.3
        EMaterialType::Sand, EMaterialType::Sand, 0.05f });

    // 凍原（tundra）：低溫，卵石地表，幾乎無樹木
    Biomes.Add({ "Tundra",
        -1.0f, 0.1f,    // Temp: -1.0 ~ 0.1
        -1.0f, 0.6f,    // Humid: -1.0 ~ 0.6
        EMaterialType::Stone_Cobble, EMaterialType::Stone_Cobble, 0.1f });

    // 沼澤（swamp）：中溫高濕，草地，低矮稀疏樹木
    Biomes.Add({ "Swamp",
         0.2f, 0.7f,    // Temp: 0.2 ~ 0.7
         0.6f, 1.0f,    // Humid: 0.6 ~ 1.0
        EMaterialType::Grass, EMaterialType::Dirt_Dry, 0.6f });

    // 高地（highland）：中低溫中濕，灰燼地表，稀疏樹木
    Biomes.Add({ "Highland",
        -0.2f, 0.4f,    // Temp: -0.2 ~ 0.4
         0.1f, 0.7f,    // Humid: 0.1 ~ 0.7
        EMaterialType::Ash, EMaterialType::Stone_Cobble, 0.2f });
}

// 查詢（const，thread-safe，無鎖）：遍歷找第一個 Temp/Humid 範圍都符合的群系
const FBiomeDef& FBiomeRegistry::Query(float T, float H)
{
    for (const FBiomeDef& B : Biomes)
        if (T >= B.TempMin && T < B.TempMax && H >= B.HumidMin && H < B.HumidMax)
            return B;
    return GetDefault();
}

const FBiomeDef& FBiomeRegistry::GetDefault()
{
    check(!Biomes.IsEmpty());
    return Biomes[0];  // Temperate
}
