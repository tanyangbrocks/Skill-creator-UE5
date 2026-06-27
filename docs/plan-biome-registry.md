# FBiomeRegistry 生物群系實作計畫

> 撰寫日期：2026-06-28  
> 依據：`docs/plugin-assessment.html` §「FBiomeRegistry 生物群系」  
> 關鍵優勢：FastNoiseLite 已在 `MapGenerator3D.cpp` 中，Zero 外部依賴，  
>           整合點清晰（`ComputeChunkData()` 靜態函數 + `PlantTreesForChunk()`）。

---

## 一、現有架構評估

### ComputeChunkData()（背景 thread，純靜態函數）

```
Seed → 高度圖 noise → SurfaceY
SurfaceY → 材質賦值：
    Y == SurfaceY → Grass（硬編碼）
    SurfaceY < Y ≤ SurfaceY+3 → Dirt_Dry（硬編碼）
    Y > SurfaceY+3 → Stone_Cobble（硬編碼）
```

**問題**：地表材質完全固定，沙漠/雪地/沼澤等不同地貌無法表達。

### 整合限制
- `ComputeChunkData()` 是 `static` 函數，接受 `Seed` 參數
- FastNoiseLite 已 include（THIRD_PARTY_INCLUDES_START）
- 不能傳指針或引用到 `FMapGenerator3D` 成員（背景 thread，所有資料必須在閉包內）
- **解法**：biome noise 的兩個 seed 可從現有 `Seed` 衍生（`Seed ^ 0xFEEDF00D`），  
  在 `ComputeChunkData()` 內部構造 FastNoiseLite → 無需改函數簽名，無需額外閉包資料

---

## 二、生物群系設計

### FBiomeDef（5 種初始群系）

| 群系 | 溫度 | 濕度 | 地表材質 | 次表面材質 | 樹木密度乘數 |
|------|------|------|----------|-----------|------------|
| Temperate（溫帶） | 0.2~0.6 | 0.3~0.7 | Grass | Dirt_Dry | 1.0 |
| Desert（沙漠） | 0.7~1.0 | 0.0~0.3 | Sand | Sand | 0.05 |
| Tundra（凍原） | -1.0~0.1 | 0.0~0.5 | Stone_Cobble | Stone_Cobble | 0.1 |
| Swamp（沼澤） | 0.2~0.6 | 0.7~1.0 | Grass | Dirt_Dry | 0.6（低矮植物） |
| Highland（高地） | -0.1~0.3 | 0.2~0.6 | Ash | Stone_Cobble | 0.2 |

### Noise 參數
- **溫度 noise**：seed = `Seed ^ 0xFEEDF00D`，freq = 0.0003f，FBm 3 octaves  
  （低頻 → 溫度帶寬，Lac=2, Gain=0.5）
- **濕度 noise**：seed = `Seed ^ 0xC0FFEE01`，freq = 0.0005f，FBm 3 octaves  
  （略高頻 → 濕度帶更細膩，與溫度帶不完全重疊）
- 輸出 -1..1 直接作為 Temperature / Humidity 查表

---

## 三、FBiomeDef + FBiomeRegistry

### 新增檔案

| 檔案 | 說明 |
|------|------|
| `Plugins/VoxelWorld/Source/VoxelWorld/Public/BiomeRegistry.h` | FBiomeDef + FBiomeRegistry |
| `Plugins/VoxelWorld/Source/VoxelWorld/Private/BiomeRegistry.cpp` | 群系表 + Query |

### 不需改 ComputeChunkData 函數簽名

biome noise seed 從現有 `Seed` 衍生，在函數內部建立 FastNoiseLite，完全 thread-safe：

```cpp
// ComputeChunkData 內部新增（緊接在現有 FastNoiseLite 宣告後）：
FastNoiseLite TN, HN2;
TN.SetSeed(Seed ^ 0xFEEDF00D);
TN.SetFrequency(0.0003f);
TN.SetFractalType(FastNoiseLite::FractalType::FBm);
TN.SetFractalOctaves(3);

HN2.SetSeed(Seed ^ 0xC0FFEE01);
HN2.SetFrequency(0.0005f);
HN2.SetFractalType(FastNoiseLite::FractalType::FBm);
HN2.SetFractalOctaves(3);
```

---

## 四、資料結構

```cpp
// BiomeRegistry.h

#include "MaterialType.h"

struct VOXELWORLD_API FBiomeDef
{
    FName          BiomeId;
    float          TempMin, TempMax;    // -1..1
    float          HumidMin, HumidMax;  // -1..1
    EMaterialType  SurfaceMat   = EMaterialType::Grass;
    EMaterialType  SubsurfaceMat = EMaterialType::Dirt_Dry;
    float          TreeDensityMult = 1.0f;  // 傳入 PlantTreesForChunk 調整生成機率
    // 供 FDecorationRegistry 查詢（AllowedBiomes 過濾）
};

class VOXELWORLD_API FBiomeRegistry
{
public:
    static void Initialize();
    static const FBiomeDef& Query(float Temperature, float Humidity);
    static const FBiomeDef& GetDefault();  // Temperate

private:
    static TArray<FBiomeDef> Biomes;
    static bool bInitialized;
};
```

---

## 五、實作步驟

### BIO-0：BiomeRegistry.h/.cpp 空殼 + Build 通過

- 建立 `BiomeRegistry.h`（struct + class 宣告）
- 建立 `BiomeRegistry.cpp`（靜態成員定義、空 `Initialize()`、`Query()` 回傳 Temperate）
- 加入 `VoxelWorld.Build.cs` 不需新依賴（VoxelWorld 內部）

### BIO-1：Initialize() 填入初始群系表

```cpp
void FBiomeRegistry::Initialize()
{
    Biomes.Empty();

    Biomes.Add({ "Temperate", -0.1f, 0.6f, 0.2f, 0.8f,
                 EMaterialType::Grass, EMaterialType::Dirt_Dry, 1.0f });

    Biomes.Add({ "Desert",     0.5f, 1.0f, -1.0f, 0.25f,
                 EMaterialType::Sand, EMaterialType::Sand, 0.05f });

    Biomes.Add({ "Tundra",    -1.0f, 0.1f, -1.0f, 0.6f,
                 EMaterialType::Stone_Cobble, EMaterialType::Stone_Cobble, 0.1f });

    Biomes.Add({ "Swamp",      0.2f, 0.7f, 0.6f, 1.0f,
                 EMaterialType::Grass, EMaterialType::Dirt_Dry, 0.6f });

    Biomes.Add({ "Highland",  -0.2f, 0.4f, 0.1f, 0.7f,
                 EMaterialType::Ash, EMaterialType::Stone_Cobble, 0.2f });

    bInitialized = true;
}

const FBiomeDef& FBiomeRegistry::Query(float T, float H)
{
    for (const FBiomeDef& B : Biomes)
        if (T >= B.TempMin && T < B.TempMax && H >= B.HumidMin && H < B.HumidMax)
            return B;
    return GetDefault();  // Temperate fallback
}
```

### BIO-2：ComputeChunkData() 接入 biome noise

`MapGenerator3D.cpp` 的 `ComputeChunkData()` 修改：

1. 在現有 noise 宣告後加 biome noise（TN / HN2）
2. 在每格計算 `SurfaceY` 後，在該格的 (wx, wz) 查 biome：

```cpp
// 在 SurfaceY 計算後，wx/wz 已知時查詢
float Temp  = TN.GetNoise((float)wx, (float)wz);   // -1..1
float Humid = HN2.GetNoise((float)wx, (float)wz);  // -1..1
const FBiomeDef& Biome = FBiomeRegistry::Query(Temp, Humid);
```

3. 材質賦值改為 biome-aware：

```cpp
// 原：EMaterialType::Grass
// 改：
if (wy == EffSurfaceY)
{
    MatID = bPoolHit ? (uint8)PoolMat : (uint8)Biome.SurfaceMat;
}
else if (wy > EffSurfaceY && wy <= EffSurfaceY + 3)
{
    MatID = (uint8)Biome.SubsurfaceMat;
}
// 石頭層不變（Stone_Cobble 是基礎地質，所有群系相同）
```

> **注意**：`FBiomeRegistry::Initialize()` 必須在 `ComputeChunkData()` 被呼叫前完成（即 `InitializeWorldState()` 中）。  
> `FBiomeRegistry::Query()` 是 const 純函數，thread-safe。

### BIO-3：PlantTreesForChunk() 接入 biome 密度

現有 `PlantTreesForChunk()` 有固定 20% 機率（`RandF() > 0.20f`）。  
改為查詢 biome 的 `TreeDensityMult`：

```cpp
// PlantTreesForChunk 函數簽名（加 Biome noise 參數）：
static void PlantTreesForChunk(FTileWorld3D& World, FIntVector CC, int32 Seed);
// 在函數內部建立 biome noise（同 ComputeChunkData，從 Seed 衍生），查詢 XZ biome → 取 TreeDensityMult
float BaseTreeProb = 0.20f;
float Prob = BaseTreeProb * Biome.TreeDensityMult;  // 沙漠：0.01，溫帶：0.20，沼澤：0.12
if (RandF() > Prob) continue;
```

### BIO-4：FBiomeRegistry::Initialize() 呼叫時機

在 `AVoxelWorldActor::InitializeWorldState()` 加：
```cpp
FBiomeRegistry::Initialize();
```

（與 DECO-5 的 `FDecorationRegistry::Initialize()` 並排呼叫）

### BIO-5：Build + 遊戲驗證

Build 通過後，在無限世界中探索：
- 低濕度高溫度區域 → 地表應為 Sand
- 低溫區域 → 地表應為 Stone_Cobble（Tundra）
- 沼澤（中溫高濕）→ 樹木密度降低

---

## 六、新增群系的 SOP（完成後）

1. 在 `BiomeRegistry.cpp` 的 `Initialize()` 加一行 `Biomes.Add({...})`
2. 調整 TempMin/TempMax/HumidMin/HumidMax 劃分
3. 指定 SurfaceMat / SubsurfaceMat（必須是現有 EMaterialType 值）
4. Build（.cpp 改，可 Live Coding）

---

## 七、與 FDecorationRegistry 的整合（BIO→DECO）

`FBiomeRegistry::Query(T, H)` 回傳的 `FBiomeDef::BiomeId`，  
可供 DECO 系統的 `AllowedBiomes` 過濾：

```cpp
// OnChunkApplied 中，取到 Biome 後：
if (!Def.AllowedBiomes.IsEmpty() && !Def.AllowedBiomes.Contains(Biome.BiomeId))
    continue;  // 仙人掌只在 Desert，雪晶只在 Tundra
```

---

## 八、完成定義

- [ ] BIO-0：BiomeRegistry.h/.cpp 空殼 + Build 通過
- [ ] BIO-1：Initialize() 填入 5 種初始群系
- [ ] BIO-2：ComputeChunkData() biome noise + 地表材質 biome-aware（0 warning）
- [ ] BIO-3：PlantTreesForChunk() TreeDensityMult 接入
- [ ] BIO-4：InitializeWorldState() 呼叫 FBiomeRegistry::Initialize()
- [ ] BIO-5：遊戲驗證：可在地圖上觀察到明顯的群系邊界
