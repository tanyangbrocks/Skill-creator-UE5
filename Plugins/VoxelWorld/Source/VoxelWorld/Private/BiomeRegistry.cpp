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

    // ── 特化群系先行（避免被 Temperate 大範圍吃掉）────────────────────────

    // 沙漠（desert）：高溫低濕，沙地，幾乎無樹木
    Biomes.Add({ "Desert",
         0.45f, 1.0f,   // Temp: 0.45 ~ 1.0
        -1.0f, 0.35f,   // Humid: -1.0 ~ 0.35
        EMaterialType::Sand, EMaterialType::Sand, 0.05f });

    // 凍原（tundra）：低溫，卵石地表，幾乎無樹木
    Biomes.Add({ "Tundra",
        -1.0f, 0.0f,    // Temp: -1.0 ~ 0.0
        -1.0f, 0.7f,    // Humid: -1.0 ~ 0.7
        EMaterialType::Stone_Cobble, EMaterialType::Stone_Cobble, 0.1f });

    // 沼澤（swamp）：中溫高濕，草地，低矮稀疏樹木
    Biomes.Add({ "Swamp",
         0.1f, 0.7f,    // Temp: 0.1 ~ 0.7
         0.55f, 1.0f,   // Humid: 0.55 ~ 1.0
        EMaterialType::Grass, EMaterialType::Dirt_Dry, 0.6f });

    // 高地（highland）：中低溫中濕，灰燼地表，稀疏樹木
    Biomes.Add({ "Highland",
        -0.1f, 0.35f,   // Temp: -0.1 ~ 0.35
         0.15f, 0.6f,   // Humid: 0.15 ~ 0.6
        EMaterialType::Ash, EMaterialType::Stone_Cobble, 0.2f });

    // 溫帶（temperate）：最後作為 fallback，覆蓋其餘所有未匹配區域
    Biomes.Add({ "Temperate",
        -1.0f, 1.0f,    // Temp: 全範圍 fallback
        -1.0f, 1.0f,    // Humid: 全範圍 fallback
        EMaterialType::Grass, EMaterialType::Dirt_Dry, 1.0f });
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
    return Biomes.Last();  // Temperate（最後一個，全範圍 fallback）
}
