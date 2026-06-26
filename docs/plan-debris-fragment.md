# 碎塊與碎片系統實作計畫

> 最後更新：2026-06-23

---

## 一、術語

| 術語 | 說明 |
|------|------|
| **碎塊（Debris）** | 物理飛行 Actor，從爆炸/斬擊/碾壓產生，有重力/彈跳，可撿起/投擲（未來） |
| **碎片（Fragment）** | 庫存物品 `FragmentXxx`（`EItemId`），撿起碎塊後轉換而來 |
| **掉落物（DroppedItem）** | `ADroppedItemActor`：靜態、無物理、採礦/箱子掉落用，與碎塊是不同系統 |

---

## 二、現況盤點

### ✅ 已完成

| 項目 | 位置 |
|------|------|
| `EDestroyReason`：Mining / ShapeMining / Explosion / Slash / Crush | `SkillCreatorCore/Public/WorldTypes.h` |
| `SpawnForReason()`：Explosion per-tile 跳過，Mining/Slash/Crush 走材質掉落表 | `UDroppedItemManager.cpp:65` |
| `SpawnFragments()`：tileCount → FragmentXxx 計算（Mining / Explosion） | `UDroppedItemManager.cpp:95` |
| `FMaterialRegistry::GetFragmentItem()` | MaterialRegistry |
| `FragmentXxx` ItemId × 9 | `SkillCreatorCore/Public/ItemId.h` |
| Fragment → Block 配方 × 9（100 碎片 → 1 方塊/礦石） | `RecipeRegistry.cpp` |

### ❌ 缺口

| 項目 | 說明 |
|------|------|
| **`ADebrisActor` 不存在** | 目前只有靜態 `ADroppedItemActor`，沒有碎塊物理飛行效果 |
| **Slash / Crush 未呼叫 SpawnFragments** | 這兩個 Reason 走一般掉落表，不產碎片 |
| **爆炸聚合缺口** | `SpawnForReason(Explosion)` 已跳過 per-tile，但沒有人在爆炸結束後呼叫 `SpawnFragments()` |

---

## 三、架構決策

### ADebrisActor vs ADroppedItemActor

```
ADroppedItemActor   靜態放置於地面，玩家靠近自動撿取 → 物品欄
ADebrisActor        有 UE5 剛體物理（SimulatePhysics=true），生成時給予衝量飛出
                    落地後玩家靠近撿取 → 計算 FragmentXxx count → 加入物品欄
```

兩者不合併。`ADebrisActor` 未來 Chaos Fracture 模式下可包裝幾何碎片 mesh，
`ADroppedItemActor` 保持原本的簡單掉落邏輯。

### 爆炸聚合路徑

`TileWorld3D::Explode()` 目前每個 tile 各呼叫 `OnTileDestroyed`，無聚合。
解法：修改 `Explode()` 在內部累計 `TMap<EMaterialType, int32> DestroyedCount`，
爆炸結束後透過新的 `OnExplodeComplete` 回呼一次性回報。

```
Explode(cx,cy,cz,r)
  └─ per tile: SetTile→Air, OnTileDestroyed(per-tile, Reason=Explosion)
  └─ 結束: OnExplodeComplete(center, TMap<Mat, TileCount>)
             └─ SpawnDebris(center, mat, tileCount)  ← 新方法
                  └─ Spawn ADebrisActor + LaunchImpulse
                  └─ 儲存 FragmentCount 在 actor 上
```

---

## 四、碎片數量公式

```
1 material unit = 1331 tiles = 100 碎片（11³，對應 DefaultShapeRadius=5 的 Cube）
threshold：tileCount < 13（< 1% of 1331）不 spawn（太小不形成碎塊實體）

碎片數 = (tileCount / 1000)
       × BaseRate(Reason)       // 見下表
       × Intensity              // 威力 0~1，由呼叫端傳入（技能傷害/爆炸半徑換算）
       × Brittleness(Mat)       // 材質脆度，存於 FMaterialData（水晶=1.2, 石=1.0, 木=0.8, 土=0.6）
       × AngleFactor(Dir, Mat)  // 僅 Slash 有效：垂直入射=0.7（切乾淨），斜角=1.3（碎更多）
```

| DestroyReason | BaseRate | 說明 |
|--------------|----------|------|
| Mining | 100 | 完整回收，不受 Intensity / AngleFactor 影響 |
| ShapeMining | 100 | 同上 |
| Explosion | Rand(20, 80) | 受 Intensity（爆炸威力）和 Brittleness 影響 |
| Slash | Rand(30, 70) | 受 Intensity（斬擊力道）、AngleFactor 和 Brittleness 影響 |
| Crush | Rand(50, 100) | 受 Intensity（重量/速度）和 Brittleness 影響；碾壓保留較多 |

> `Brittleness` 加入 `FMaterialData`（`MaterialRegistry.h`）。
> 位置因素（距爆炸中心遠近）已隱含在 `tileCount` 裡——`Explode()` 的 `Chance` 參數讓邊緣 tile 本就較難被摧毀，不需額外參數。
> Phase 2 Chaos Fracture 路徑：用幾何碎片體積換算 tileCount，套用同一公式。

---

## 五、Phase 1 實作項目（tile 世界，現在可做）

### D-1：`ADebrisActor` 基礎類別

位置：`Source/SkillCreatorRuntime/Public/ADebrisActor.h` + `.cpp`

```cpp
UCLASS()
class ADebrisActor : public AActor, public IInteractable
{
    UStaticMeshComponent* Mesh;  // 現在用小方塊，Phase 2 換 Chaos 幾何碎片

    EItemId FragmentItemId;   // 撿起後給的物品
    int32   FragmentCount;    // 撿起後給幾個

    void LaunchImpulse(FVector Vel);  // 給予初速度
    // IInteractable: 撿起 → 加物品欄 → Destroy()
};
```

### D-2：`TileWorld3D::Explode()` 加聚合回呼

在 `TileWorld3D.h` 加：

```cpp
TFunction<void(FIntVector Center, const TMap<EMaterialType, int32>& DestroyedByMat)>
    OnExplodeComplete;
```

`Explode()` 內部累計 `TMap<EMaterialType, int32>`，結束後呼叫此回呼。

### D-3：`AVoxelWorldActor` 訂閱 `OnExplodeComplete`

在 `BeginPlay` 或 `ASkillCreatorCharacter` 的初始化路徑裡訂閱，
呼叫 `UDroppedItemManager::SpawnDebris(Center, Mat, TileCount, EDestroyReason::Explosion)`。

### D-4：`UDroppedItemManager::SpawnDebris()` 新方法

```cpp
struct FDebrisParams
{
    EDestroyReason Reason    = EDestroyReason::Explosion;
    float          Intensity = 1.f;   // 0~1，技能威力換算
    FVector        Direction = FVector::ZeroVector;  // 斬擊方向（Slash 用）
};

// 計算碎片數（套用 Brittleness / Intensity / AngleFactor）→ spawn ADebrisActor → 給衝量
void SpawnDebris(FIntVector Center, EMaterialType Mat, int32 TileCount,
                 const FDebrisParams& Params);
```

`FMaterialRegistry` 增加 `GetBrittleness(EMaterialType)` 查詢，
`Brittleness` 值儲存於 `FMaterialData`（在 `MaterialRegistry.cpp::Initialize()` 的 `Register()` 呼叫補值）。

### D-5：補 Slash / Crush → `SpawnDebris()`

修改 `SpawnForReason()`：Slash / Crush 也呼叫 `SpawnDebris()`，
傳入適當的 `FDebrisParams`（Intensity 暫設 1.0，Direction 由攻擊方向傳入）。
不再只走材質掉落表。

---

## 六、Phase 2（Chaos Fracture，建模導入後）

> 前提：遊戲導入 3D mesh 角色 / 環境物件

- **D-6**：敵人 / NPC body mesh 設定 Chaos Fracture Geometry Collection（預切割關節）
- **D-7**：Chaos Fracture 破壞事件 → 估算各 piece 體積 → 對應碎片數 → `ADebrisActor`（ChaosMode）
- **D-8**：`ADebrisActor` 支援 ChaosMode：mesh 換成 Chaos 幾何碎片，物理由 Chaos 接管

Chaos Fracture 路徑最終也產生 `ADebrisActor`，撿起後一樣轉換成 `FragmentXxx` 物品，
與 Phase 1 的 tile 路徑在物品欄層面完全相容。

---

## 七、Phase 3+ 未來方向（另開計畫）

| 技術 | 適用場景 | 計畫文件 |
|------|---------|---------|
| **Voxelization** | 匯入 mesh 轉 tile → 走 Phase 1 路徑，mesh 物件參與 CA 世界 | [`plan-voxelization.md`](plan-voxelization.md) |
| **Chaos Cloth** | 布料 / 甲冑撕裂 → 布料碎片也可撿取 | 待建 |
| **MPM** | 軟體形變：角色踩扁、肉體碾壓、連續介質特效 | 待建 |

三條路徑最終都接入同一個 `ADebrisActor → FragmentXxx` 介面，物品系統無需改動。

---

## 八、實作順序

```
[x] D-1  ADebrisActor 基礎（物理飛行、撿起 → Fragment 物品）
[x] D-2  TileWorld3D::Explode() 聚合回呼
[x] D-3  AVoxelWorldActor 訂閱 OnExplodeComplete → SpawnDebris
[x] D-4  UDroppedItemManager::SpawnDebris() 方法
[x] D-5  SpawnForReason 補 Slash / Crush → SpawnDebris
         ↑ D-4 完成後，plan-voxelization V-4 才能接上
           （ADestructibleMeshActor::TriggerDestruction 依賴 SpawnDebris()，見 plan-voxelization §十五）
─────── Phase 2（建模導入後）───────
[ ] D-6  Chaos Fracture 幾何集合設定        ← 前置：3D 美術提供 Fracture Geometry Collection 資產
[ ] D-7  Chaos 破壞事件 → ADebrisActor      ← 前置：D-6 完成
[ ] D-8  ADebrisActor Chaos 模式            ← 前置：D-7 完成
```

> **2026-06-26 更新**：G-10（`plan-physical-item.md`）已讓 `ADebrisActor` 實作 `IPhysicalPickable`，
> 碎塊撿取→攜帶→投擲→存入物品欄的完整路徑已通。D-6~D-8 整合路徑（Chaos 模式下 `ADebrisActor`
> 仍實作同一介面，`OnCarried`/`OnReleased` 邏輯保持不變）詳見 `plan-physical-item.md §十`。
> D-6~D-8 排期的唯一前置是 3D 美術提供資產，C++ 無須另行修改介面層。
