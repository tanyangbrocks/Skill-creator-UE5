# 地表裝飾框架實作計畫（純 C++，不使用 PCG Plugin）

> 撰寫日期：2026-06-28  
> 依據：`docs/plugin-assessment.html` §「PCG 地表裝飾框架」  
> 重要決策：**不使用 UE5 PCG Plugin**，改用純 C++ Registry + ISMC 架構。  
> 目標：新增任何裝飾物件只需在 `DecorationRegistry.cpp` 加一行 `Register()`，零 Editor 操作。

---

## 一、問題背景：PCG Plugin 方案的代價

HTML 原文提到「缺點：每次 Chunk 生成後須重新觸發對應區域的 PCG Graph」。  
這個缺點在本專案中影響極大：

**觸發頻率**
- CA 模擬（水/熔岩蔓延、沙子落下）每幾秒就可能改變地表材質 → 觸發 chunk dirty → RMC 重建
- 玩家採掘/放置也觸發 chunk 重建
- 每次 RMC 重建後 PCG 就要重新 re-trigger 一遍

**每次 re-trigger 的代價**
1. Destroy 所有現有裝飾 Actor（該 chunk 範圍內的樹/石/蘑菇全部刪除）
2. PCG 重新採樣表面點（Point Sampling）
3. 自訂 PCG Node 逐格查詢 `FTileWorld3D::GetTile()` 過濾
4. 重新 Spawn 所有裝飾 Actor
- 完整執行耗時 5~50 ms（依裝飾密度），超出一幀 → 明顯卡頓

**視覺閃爍**
- Destroy → Spawn 之間至少間隔一幀：樹/石消失後再出現
- 在 CA 活躍水岸、沙漠邊緣可能每幾秒發生一次，嚴重破壞沉浸感

**狀態遺失**
- 已受損的樹（被劍砍過）Destroy 後重生變回完整原狀
- 附掛在裝飾物上的 GAS Effect / 物理狀態全部消失

**純 C++ 方案如何規避**
| 問題 | PCG Plugin | 純 C++ 方案 |
|------|-----------|-------------|
| re-trigger 時機 | RMC 重建 = PCG 重觸發 | 只在「chunk 初次生成」時觸發一次 |
| 更新成本 | Destroy + Spawn 個別 Actor | ISMC 批次增刪 Instance，< 0.5 ms |
| 視覺閃爍 | 必然（Destroy/Spawn 空窗） | 無（ISMC 實例原地保留） |
| Editor 操作 | PCG Graph Node 拖拉 + 參數設定 | 純 C++ Register() 一行 |

---

## 二、架構設計

### 新增檔案

| 檔案 | 說明 |
|------|------|
| `Plugins/VoxelWorld/Source/VoxelWorld/Public/DecorationRegistry.h` | FDecorationDef + FDecorationRegistry |
| `Plugins/VoxelWorld/Source/VoxelWorld/Private/DecorationRegistry.cpp` | 初始裝飾登錄 + 查詢 |
| `Plugins/VoxelWorld/Source/VoxelWorld/Public/UDecorationSubsystem.h` | UWorldSubsystem，ISMC 管理 |
| `Plugins/VoxelWorld/Source/VoxelWorld/Private/UDecorationSubsystem.cpp` | chunk 回呼 + 地表掃描 + placement |

### 修改檔案

| 檔案 | 修改 |
|------|------|
| `Plugins/VoxelWorld/Source/VoxelWorld/Public/AVoxelWorldActor.h` | 新增 `FOnChunkApplied` delegate |
| `Plugins/VoxelWorld/Source/VoxelWorld/Private/AVoxelWorldActor.cpp` | Tick() 中 ApplyPendingChunks 後廣播 |

### 資料流

```
背景 thread: ComputeChunkData()
     ↓ FPendingChunk 進 ReadyQueue
FMapGenerator3D::ApplyPendingChunks() [game thread]
     ↓ 寫入 TileWorld + 種樹
AVoxelWorldActor::Tick()
     ↓ 廣播 OnChunkApplied(CC)
UDecorationSubsystem::OnChunkApplied(CC) [game thread]
     ↓ 掃描 CC 範圍內的地表 tile
     ↓ 查 FDecorationRegistry 取候選裝飾
     ↓ 確定性 hash → 決定放置哪些
     ↓ AddInstance() 到對應 ISMC
```

---

## 三、關鍵資料結構

```cpp
// DecorationRegistry.h

struct VOXELWORLD_API FDecorationDef
{
    FName            PropId;           // 唯一識別（用於 ISMC 分組）
    FSoftObjectPath  MeshPath;         // /Game/Decorations/SM_Rock_Small.SM_Rock_Small
    TArray<EMaterialType> ValidSurfaces; // 允許生長的地表材質
    float            Density;          // 0..1，每個符合格的生成機率
    FVector2D        ScaleRange;       // X=MinScale, Y=MaxScale（均一縮放）
    float            MaxSlopeDeg;      // 容許坡度（0=只在平地；45=容許斜面）
    // 選填：生物群系限定（空 = 所有群系）
    TArray<FName>    AllowedBiomes;
};

class VOXELWORLD_API FDecorationRegistry
{
public:
    static void Initialize();    // 啟動時呼叫一次，Register 所有裝飾
    static void Register(const FDecorationDef& Def);
    static const TArray<FDecorationDef>& GetAll();
private:
    static TArray<FDecorationDef> Defs;
};
```

---

## 四、ISMC 管理器（UDecorationSubsystem）

```cpp
// UDecorationSubsystem.h
UCLASS()
class VOXELWORLD_API UDecorationSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    // PropId → ISMC（一種 Mesh 一個 ISMC，所有 chunk 共用）
    UPROPERTY() TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> ISMCByProp;
    // 每個 chunk 持有哪些 instance（PropId, InstanceIndex），用於 eviction 時清除
    TMap<FIntVector, TArray<TPair<FName, int32>>> ChunkInstances;

    // 訂閱 AVoxelWorldActor 的 delegate
    FDelegateHandle ChunkAppliedHandle;
    FDelegateHandle ChunkEvictedHandle;

    void OnChunkApplied(FIntVector CC);
    void OnChunkEvicted(FIntVector CC);

    UInstancedStaticMeshComponent* GetOrCreateISMC(const FDecorationDef& Def);
    // 確定性 hash（chunk 座標 + XZ offset + PropId → 生成決定）
    bool ShouldPlace(FIntVector CC, int32 LX, int32 LZ, const FDecorationDef& Def, float& OutScale) const;
};
```

---

## 五、實作步驟

### DECO-0：FDecorationDef + FDecorationRegistry（header + .cpp 空殼）

- 建立 `DecorationRegistry.h`（struct + class 宣告）
- 建立 `DecorationRegistry.cpp`（靜態成員定義、`Initialize()`/`Register()`/`GetAll()` 空殼）
- 加入 `VoxelWorld.Build.cs`（無需新依賴，VoxelWorld 內部）

### DECO-1：初始裝飾登錄（DecorationRegistry.cpp）

在 `Initialize()` 中 Register 第一批裝飾：

```cpp
void FDecorationRegistry::Initialize()
{
    // 小石頭（草地 / 泥土）
    Register({
        .PropId       = "Rock_Small",
        .MeshPath     = FSoftObjectPath(TEXT("/Game/Decorations/SM_Rock_Small.SM_Rock_Small")),
        .ValidSurfaces = { EMaterialType::Grass, EMaterialType::Dirt_Dry },
        .Density      = 0.04f,
        .ScaleRange   = { 0.8f, 1.4f },
        .MaxSlopeDeg  = 30.f,
    });

    // 蘑菇（草地）
    Register({
        .PropId       = "Mushroom_Red",
        .MeshPath     = FSoftObjectPath(TEXT("/Game/Decorations/SM_Mushroom_Red.SM_Mushroom_Red")),
        .ValidSurfaces = { EMaterialType::Grass },
        .Density      = 0.02f,
        .ScaleRange   = { 0.6f, 1.2f },
        .MaxSlopeDeg  = 15.f,
    });

    // 仙人掌（沙地）
    Register({
        .PropId       = "Cactus",
        .MeshPath     = FSoftObjectPath(TEXT("/Game/Decorations/SM_Cactus.SM_Cactus")),
        .ValidSurfaces = { EMaterialType::Sand },
        .Density      = 0.06f,
        .ScaleRange   = { 1.0f, 1.8f },
        .MaxSlopeDeg  = 10.f,
    });

    // 魔法水晶（石頭地表）
    Register({
        .PropId       = "Crystal_Blue",
        .MeshPath     = FSoftObjectPath(TEXT("/Game/Decorations/SM_Crystal_Blue.SM_Crystal_Blue")),
        .ValidSurfaces = { EMaterialType::Stone_Cobble },
        .Density      = 0.015f,
        .ScaleRange   = { 0.5f, 1.5f },
        .MaxSlopeDeg  = 45.f,
    });
}
```

**注意**：Mesh 資產本身仍需在 UE Editor 中匯入一次（FBX 匯入到 /Game/Decorations/），  
但這與 PCG Node 設定完全不同：匯入是一次性的資產操作，之後新增裝飾只改 C++。

### DECO-2：AVoxelWorldActor delegate

**`AVoxelWorldActor.h`** 新增（在 `FOnExplosionCompleteDelegate` 附近）：
```cpp
DECLARE_MULTICAST_DELEGATE_OneParam(FOnChunkAppliedDelegate, FIntVector);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnChunkEvictedDelegate, FIntVector);

// public:
FOnChunkAppliedDelegate OnChunkApplied;
FOnChunkEvictedDelegate OnChunkEvicted;
```

**`AVoxelWorldActor.cpp`** Tick() 中：
```cpp
// 原有：
Streaming.Tick(TileWorld, CX, CY, CZ, StreamRadius, CARadius);

// 改為：在 ApplyPendingChunks 後廣播
int32 PrevPending = Streaming.GetMapGenerator().GetPendingChunkCount();
Streaming.Tick(TileWorld, CX, CY, CZ, StreamRadius, CARadius);
// 已套用 = 之前 pending 的 chunk 數 - 現在 pending 的數量（每幀最多套用 MaxPerFrame 個）
// 較簡單：直接讓 FMapGenerator3D::ApplyPendingChunks 廣播 delegate
```

更簡潔的做法：在 `FMapGenerator3D::ApplyPendingChunks()` 末尾加一個 callback：

```cpp
// MapGenerator3D.h 新增成員：
TFunction<void(FIntVector)> OnChunkAppliedCallback;

// ApplyPendingChunks 末尾：
++Applied;
if (OnChunkAppliedCallback) OnChunkAppliedCallback(CC);
```

在 `ChunkStreamingManager.h` 裡把 callback 傳入 MapGen：
```cpp
MapGen.OnChunkAppliedCallback = [this](FIntVector CC){ /* forward to AVoxelWorldActor */ };
```

或最直接的：在 `AVoxelWorldActor::Tick()` 比較 `CreatedMegaChunks` before/after 推算新增 chunk（有現成的 `FIntVector MegaChunkCoord` 迴圈邏輯可利用）。

### DECO-3：UDecorationSubsystem 實作

**Initialize()**：
1. `FDecorationRegistry::Initialize()` — 確保一次登錄
2. 從 World 找 `AVoxelWorldActor::FindInWorld(GetWorld())`
3. 綁定 `OnChunkApplied` delegate：`VWA->OnChunkApplied.AddUObject(this, &UDecorationSubsystem::OnChunkApplied)`
4. 綁定 `OnChunkEvicted` delegate（`AVoxelWorldActor::Tick()` 中 SaveAndEvict 後廣播）

**OnChunkApplied(FIntVector CC)**：

```cpp
void UDecorationSubsystem::OnChunkApplied(FIntVector CC)
{
    constexpr int32 S = WorldScale::ChunkSize;
    AVoxelWorldActor* VWA = AVoxelWorldActor::FindInWorld(GetWorld());
    if (!VWA) return;

    FTileWorld3D* TW = VWA->GetTileWorld();
    const TArray<FDecorationDef>& AllDefs = FDecorationRegistry::GetAll();

    for (int32 lz = 0; lz < S; ++lz)
    for (int32 lx = 0; lx < S; ++lx)
    {
        int32 wx = CC.X * S + lx;
        int32 wz = CC.Z * S + lz;

        // 找「最高實心格正上方的 Air 格」= 地表
        int32 SurfaceAirY = -1;
        for (int32 wy = CC.Y * S; wy < (CC.Y + 1) * S; ++wy)
        {
            EMaterialType Mat = TW->GetTile(wx, wy, wz);
            if (Mat == EMaterialType::Air) { SurfaceAirY = wy; break; }
        }
        if (SurfaceAirY < 0 || SurfaceAirY >= (CC.Y + 1) * S - 1) continue;

        EMaterialType SurfaceMat = TW->GetTile(wx, SurfaceAirY + 1, wz);
        if (SurfaceMat == EMaterialType::Air) continue;  // 懸空

        for (const FDecorationDef& Def : AllDefs)
        {
            // 地表材質符合
            if (!Def.ValidSurfaces.Contains(SurfaceMat)) continue;

            float Scale = 1.f;
            if (!ShouldPlace(CC, lx, lz, Def, Scale)) continue;

            UInstancedStaticMeshComponent* ISMC = GetOrCreateISMC(Def);
            if (!ISMC) continue;

            FVector WorldPos(
                wx * WorldScale::TileSizeCm,
                SurfaceAirY * WorldScale::TileSizeCm,   // UE Y 是向下（Y 大 = 越深），但Actor Z=Up
                wz * WorldScale::TileSizeCm
            );
            // 實際 UE5 座標轉換：tile(wx, wy, wz) → (wx*TSz, wz*TSz, -wy*TSz) 或依 WorldScale 設計
            FTransform T(FRotator(0, FMath::FRandRange(0, 360), 0), WorldPos, FVector(Scale));
            int32 Idx = ISMC->AddInstance(T);
            ChunkInstances.FindOrAdd(CC).Emplace(Def.PropId, Idx);
        }
    }
}
```

**ShouldPlace()**：純 hash 確定性決定（無 RNG 狀態）：

```cpp
bool UDecorationSubsystem::ShouldPlace(FIntVector CC, int32 LX, int32 LZ,
                                        const FDecorationDef& Def, float& OutScale) const
{
    // 確定性 hash：chunk + local offset + PropId hash
    uint32 H = (uint32)(CC.X * 1664525u ^ CC.Z * 22695477u ^ (uint32)LX * 1013904223u
                        ^ (uint32)LZ * 2654435761u ^ GetTypeHash(Def.PropId));
    H = H * 1664525u + 1013904223u;
    float F = float(H & 0xFFFF) / 65535.f;  // 0..1

    if (F >= Def.Density) return false;  // 未達密度門檻

    // Scale 從 hash 決定
    H = H * 1664525u + 1013904223u;
    OutScale = FMath::Lerp(Def.ScaleRange.X, Def.ScaleRange.Y,
                           float(H & 0xFFFF) / 65535.f);
    return true;
}
```

**OnChunkEvicted(FIntVector CC)**：

```cpp
void UDecorationSubsystem::OnChunkEvicted(FIntVector CC)
{
    TArray<TPair<FName, int32>>* Instances = ChunkInstances.Find(CC);
    if (!Instances) return;

    // 逆序移除 instance（高 index 先刪，避免 index 位移問題）
    // 注意：ISMC RemoveInstance 會把最後一個 instance swap 到被刪的位置
    // 需要更新其他 ChunkInstances 裡對應的 index → 改用 MarkRenderStateDirty 或 ClearInstances 重建
    // 簡化方案：per-PropId 重建（先 ClearInstances，再把其他 chunk 的 instance 重新 AddInstance）
    // 進階方案：維護 per-ISMC 的 chunkMask array，O(1) 刪除
    // 初版使用「定期全量重建」策略：每 N 次 eviction 後重建一次 ISMC
    for (auto& [PropId, _] : *Instances) { /* 標記 dirty */ }
    ChunkInstances.Remove(CC);
    RebuildISMCsFromScratch();  // 初版，後續可優化
}
```

> **ISMC Index 問題說明**：`RemoveInstance()` 會把最後一個 instance swap 到刪除位置，  
> 導致其他 chunk 的 index 失效。初版用「eviction 時全量重建」，後續可改為：  
> 每個 instance 記錄實際 world position 而非 index，重建時按 position 找對應 instance。

### DECO-4：坡度過濾（MaxSlopeDeg）

```cpp
// 地表坡度判斷（計算四鄰格高度差）
float UDecorationSubsystem::GetSlopeDeg(FTileWorld3D* TW, int32 wx, int32 SurfaceAirY, int32 wz) const
{
    // 找 ±1 格的地表高度
    auto GetSY = [&](int32 X, int32 Z) -> int32 {
        for (int32 y = SurfaceAirY - 2; y <= SurfaceAirY + 2; ++y)
            if (TW->GetTile(X, y, Z) != EMaterialType::Air) return y;
        return SurfaceAirY;
    };
    int32 DH = FMath::Max({
        FMath::Abs(GetSY(wx+1, wz) - SurfaceAirY),
        FMath::Abs(GetSY(wx-1, wz) - SurfaceAirY),
        FMath::Abs(GetSY(wx, wz+1) - SurfaceAirY),
        FMath::Abs(GetSY(wx, wz-1) - SurfaceAirY),
    });
    // DH tile = DH * TileSizeCm；斜率 = DH / 1（1 tile 水平距離），角度 = atan(DH)
    return FMath::RadiansToDegrees(FMath::Atan(float(DH)));
}
```

### DECO-5：FDecorationRegistry::Initialize() 呼叫時機

在 `AVoxelWorldActor::InitializeWorldState()` 末尾（或 `UDecorationSubsystem::Initialize()`）呼叫一次：
```cpp
FDecorationRegistry::Initialize();
```

---

## 六、新增裝飾物件的 SOP（完成後）

1. 在 UE Editor 匯入 FBX 到 `/Game/Decorations/SM_YourProp`（一次性）
2. 在 `DecorationRegistry.cpp` 的 `Initialize()` 加一行 `Register({...})`
3. Build（.cpp 只改，可 Live Coding）
4. 下次進遊戲，新裝飾自動出現在符合條件的地表 tile 上

---

## 七、完成定義

- [ ] DECO-0：DecorationRegistry.h/.cpp 空殼 + Build 通過
- [ ] DECO-1：Register 4 種初始裝飾（Rock/Mushroom/Cactus/Crystal）
- [ ] DECO-2：AVoxelWorldActor OnChunkApplied / OnChunkEvicted delegate
- [ ] DECO-3：UDecorationSubsystem 訂閱 delegate + OnChunkApplied 地表掃描
- [ ] DECO-4：ShouldPlace 確定性 hash + 坡度過濾
- [ ] DECO-5：OnChunkEvicted 清除（初版全量重建）+ Build 通過 0 error 0 warning
