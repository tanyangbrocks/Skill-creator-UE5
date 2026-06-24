# More World Setting 實作計畫

> 來源：`origin text setting/more world setting.txt`
> 建立日期：2026-06-25

---

## 一、世界觀規模層級（純設計資料，暫不實作生成邏輯）

```
世界 → 區域（大/小/主勢力） → 生物群系 → 地形或大結構 → 地貌或小結構
```

### 大區域（8 個）
天輝大陸、瑞澤大陸、璇霄大陸、夢之殤、魔域群島、萬魔之巔、幽影大陸、無盡海地區

### 天輝大陸小區域
東域、西域、南域、北域、半天之顛、眾生之源

### 東域主勢力層級
若夢埃頌元境紫雲宸聖天帝國（六龍聖天帝國）、東方修真界六宗十二派、天北百州

---

## 二、生物群系

| 群系類型 | 子類別 |
|---------|-------|
| 文明生物群系 | 城鎮、建築（細節待定） |
| 森林生物群系 | 櫻花木森林、橡木森林 |
| 叢林生物群系 | 漿果叢林 |
| 草原生物群系 | 青綠草原、蒼莽草原 |
| 沙漠生物群系 | 黃土沙漠 |
| 山地生物群系 | 尖鋒山地 |
| 海洋生物群系 | 熱帶海洋 |
| 雪原生物群系 | 平坦雪原 |
| 濕地生物群系 | 沼澤濕地 |
| 惡地生物群系 | 紅土惡地 |
| 地下生物群系 | 礦坑地下 |
| 地穴生物群系 | 磁場地穴 |
| 深層地穴生物群系 | 遠古深層地穴 |
| 世界核心生物群系 | 一號世界核心 |
| 地獄生物群系（世界最底部） | 熔岩地獄 |

---

## 三、地形或大結構

水晶洞窟、蒼鬱洞窟、蜘蛛洞穴、鐘乳石洞穴、岩漿湖、鬼族據點、哥布林據點

---

## 四、地貌或小結構

> ⚠️ 此層級可能會放入建模！

水池（現有實作為地貌層級）、小屋、地下軌道、舊址、地下河川、遠古傳送陣（不由演算法生成，指定位置）

---

## 五、新增材質

### 樹木（橡木）
- 區分**樹幹**（`Wood`）與**樹葉**（`Leaves`）
- 決策 1：落葉（`FallenLeaf`）為**可放置可塑形的 tile 材質**，需新增 `EMaterialType::FallenLeaf`
- 樹幹被挖掘後，整顆樹倒塌，全部化為掉落物
  - 樹幹掉落：`OakLog`（原木）
  - 樹葉掉落：`FallenLeaf`（作為**物品**；落在地上的 tile 也是同名材質）
  - 樹葉機率額外掉落：`OakSapling`（橡木樹苗）、`OakFruit`（橡木果實）
- 目前所有樹木均為橡木，後續再細分品種

### 石頭（決策 2：選 B，建立長期正確架構）
- 拆分現有 `Stone` → `Stone_Cobble`（圓石，當前唯一類型）
- 舊 `Stone` EMaterialType 不再使用，以 `Stone_Cobble` 取代
- 後續新種類（頁岩、花崗岩等）直接追加新 EMaterialType 值

### 泥土（決策 2：同上）
- 拆分現有 `Dirt` → `Dirt_Dry`（乾泥，當前唯一類型）
- 舊 `Dirt` EMaterialType 不再使用，以 `Dirt_Dry` 取代
- 後續新種類直接追加

### 草地
- 覆蓋在泥土表面
- 泥土裸露在空氣中，經過 **3 遊戲小時（= 現實 3 分鐘）** 後重新長出草地
- 沒雜草的草地有機率長出**雜草**（採集物實體）

### 暫不實作（先建資料）
- 仙人掌（採集物）
- 竹子（採集物）
- 甘蔗（採集物）

---

## 六、採掘動作細分

原「長按左鍵採掘」拆為三種動作：

| 名稱 | 觸發條件 | 採掘時間 | 結果 |
|------|----------|---------|------|
| **挖掘** | 目標是可塑形 tile（泥土/石頭類） | 依 Hardness | tile 移除 + 掉落物 |
| **拆除** | 目標是 PlacedUnit / Fixture | 依 Hardness | Actor 移除 + 掉落物 |
| **採集** | 目標是採集物實體（雜草等） | 0.5 秒（固定，可變動） | Entity 移除 + 掉落物 |

> ⚠️ 未來可能新增「採收」概念，與「採集」是不同動作，留意區分。

---

## 七、架構決策紀錄

### 決策 1：落葉為 tile 材質
`FallenLeaf` 同時作為 `EMaterialType`（tile）和 `EItemId`（物品）兩種形態存在。
拾取時轉為物品，放置時還原為 tile，行為類似 Sand/Water。

### 決策 2：石頭/泥土做真正的子類型拆分
不只改顯示名，而是廢棄舊 `Stone`/`Dirt` ID、以 `Stone_Cobble`/`Dirt_Dry` 取代。
這樣未來新增花崗岩、砂質土等只需追加新 EMaterialType，不需要改現有材質行為。
**所有原本讀寫 `EMaterialType::Stone`/`Dirt` 的地方都需要一次性替換為新 ID。**

### 決策 3：樹木跨 chunk 的生成策略
- Chunk = 16×16×16 tiles；樹高 8–10 tiles，幾乎必然跨越 chunk Y 邊界
- `ComputeChunkData`（背景 thread）**不**負責放樹，只生成地形
- `ApplyPendingChunks`（主執行緒）在 memcpy 套用後，立刻呼叫 `PlantTreesForChunk(World, CC, Seed)`
- `PlantTreesForChunk` 用 `World.SetTile()` 跨 chunk 寫入 Wood/Leaves，在主執行緒完全 thread-safe
- **保護機制**：`ApplyPendingChunks` 的 memcpy 前先檢查，若現有 tile 為 `Wood` 或 `Leaves`（由鄰居 chunk 的樹已寫入），跳過覆寫，避免後生成的地形把樹磨掉

### 決策 4：樹木為純 tile，以間距保證個體性
- 樹木 = Wood + Leaves tile 組合，不建立 `ATreeActor`
- 生成時（世界生成 + 樹苗生長）均強制樹幹間距 ≥ **6 tiles**
  - 樹冠半徑 2，兩棵樹冠之間保留 ≥ 2 tile 空氣隔斷
  - BFS 搜尋連通 Wood/Leaves 時，空氣天然截斷邊界，不會蔓延至鄰樹
- **樹苗生長機制**：random tick 命中樹苗時，先檢查 6-tile 半徑內是否已有 Wood/Leaves；若空間不足，本次不生長，等待下一次 random tick
  - 太近的樹苗永遠長不大（符合直覺，也對應 MC 行為）

### 決策 5：雜草採用 MC random tick 機制
每 game tick（≈1/20 秒），對每個已載入 chunk 的每個 16×16×16 section 隨機抽 **3 個 tile**。
命中 Grass + 上方 Air → 以 **10%** 機率 spawn `AWeedEntity`。

數字依據：
- 每 tile 被抽中機率 = 3 / 4096 ≈ 0.07% / tick
- 每 tile 平均每 **68 秒**才得到一次 random tick
- 加上 10% 成功率 → 平均 **11 分鐘**長一棵雜草
- 長出後上方不再是 Air，自動封頂，密度天然不會填滿整片草地

---

## 八、具體實作項目

### W-A　材質子類型拆分 + 落葉 tile + 新物品（.h 修改，需 Rebuild）

- [ ] `MaterialType.h`：
  - 新增 `Stone_Cobble`、`Dirt_Dry`、`FallenLeaf` 三個 EMaterialType 值
  - 舊 `Stone`（ID=1）、`Dirt`（ID=2）標記為 deprecated（保留 ID 不動以免現有存檔損毀，但所有新程式碼改用新 ID）
- [ ] `MaterialRegistry.cpp`：
  - 為 `Stone_Cobble`、`Dirt_Dry`、`FallenLeaf` 填入 `FMaterialData`（物理/硬度/掉落物等）
  - `Wood` 掉落改為 `OakLog`；`Leaves` 主掉落改為 `FallenLeaf`（物品），機率掉落 `OakSapling`/`OakFruit`
- [ ] `ItemId.h`：新增 `OakLog`、`FallenLeaf`（物品態）、`OakSapling`、`OakFruit`
- [ ] `ItemRegistry.cpp`：為上述填入 `FItemData`
- [ ] 全域搜尋替換：`EMaterialType::Stone` → `EMaterialType::Stone_Cobble`，`EMaterialType::Dirt` → `EMaterialType::Dirt_Dry`（`MapGenerator3D`、`MaterialRegistry`、`TileWorld3D` 等所有用到的地方）

### W-B　樹木生成（主執行緒後處理，純 cpp）

- [ ] `MapGenerator3D.h`：新增 `PlantTreesForChunk(FTileWorld3D&, FIntVector CC, int32 Seed)` 宣告
- [ ] `MapGenerator3D.cpp`：實作 `PlantTreesForChunk`
  - 以 Seed + CC 確定性隨機決定此 chunk 地表的樹木位置，樹幹間距 ≥ 6
  - 樹幹（`Wood`）向上（Y 減小）5–7 tiles
  - 樹冠（`Leaves`）頂部 3 層，半徑 2 的球形
  - 使用 `World.SetTile()` 跨 chunk 寫入，主執行緒安全
- [ ] `MapGenerator3D.cpp`：`ApplyPendingChunks` 的 memcpy 後插入 `PlantTreesForChunk` 呼叫
- [ ] `ApplyPendingChunks` memcpy 邏輯：逐 tile 複製時，若目標 tile 已為 `Wood`/`Leaves`，跳過

### W-C　樹木倒塌（純 cpp）

- [ ] `AVoxelWorldActor.cpp`（或 `TileWorld3D` 的 `OnTileDestroyed` 接線處）：
  - 偵測 `Wood` tile 被採掘時啟動倒塌
  - BFS 搜尋連通的 `Wood` / `Leaves`（天然以空氣為邊界，不會連鎖至鄰樹）
  - 全部設為 `Air`，對每個被移除的 tile 生成對應掉落 Actor

### W-D　採掘動作細分（純 cpp）

- [ ] 區分挖掘（可塑形 tile）/ 拆除（PlacedUnit/Fixture）/ 採集（Entity）三條判斷路徑
- [ ] 採集動作固定 0.5 秒（採集時間可設定，預留欄位）
- [ ] 採集路徑暫 stub，待 W-G 雜草 Entity 完成後接入

### W-E　草地回長計時（純 cpp）

- [ ] 新增 `FGrassRegrowthSystem`，掛在 `AVoxelWorldActor`
- [ ] `Dirt_Dry` tile 上方變為 Air 時，加入候選隊列 + 記錄時間戳
- [ ] 累計 180 秒（= 3 遊戲小時 = 現實 3 分鐘）且上方仍 Air → `SetTile(Grass)`

### W-F　生物群系 / 地形 / 地貌 資料 Enum（.h 新建，需 Rebuild）

- [ ] 新建 `WorldHierarchy.h`（`SkillCreatorCore`）
  - `EWorldRegion`（8 大區域）
  - `EBiomeType`（15 種群系）
  - `ETerrainFeature`（7 種地形大結構）
  - `ELandform`（6 種地貌小結構）
- 此階段僅聲明資料，**不接任何生成邏輯**

### W-G　雜草採集物實體（.h 新建，需 Rebuild）

- [ ] `ItemId.h`：新增 `EItemId::Weed`
- [ ] 新建 `AWeedEntity`（Actor，`SkillCreatorRuntime`）
  - random tick 命中 Grass + 上方 Air → 10% 機率 spawn
  - 採集條件：徒手或手持非工具，長按左鍵 0.5 秒
  - 結果：Actor 移除 + 掉落 `Weed`
- [ ] 接入 W-D 採集路徑

---

## 九、實作順序

```
W-A（材質子類型 + 落葉 tile + 新物品）
  ↓
W-B（樹木生成 — ApplyPendingChunks 後處理）
  ↓
W-C（樹木倒塌 BFS）
  ↓
W-D（挖掘/拆除/採集 動作細分）
  ↓
W-E（草地回長 180 秒計時）
  ↓
W-F（世界層次資料 enum）
  ↓
W-G（雜草 random tick + AWeedEntity）
```

---

## 十、.h 修改彙整（需關 Editor + 完整 Rebuild）

| 步驟 | 檔案 | 異動 |
|------|------|------|
| W-A | `MaterialType.h` | 新增 `Stone_Cobble`、`Dirt_Dry`、`FallenLeaf` |
| W-A | `ItemId.h` | 新增 `OakLog`、`FallenLeaf`（物品）、`OakSapling`、`OakFruit` |
| W-B | `MapGenerator3D.h` | 新增 `PlantTreesForChunk` 宣告 |
| W-F | `WorldHierarchy.h`（新建） | 4 個 UENUM |
| W-G | `ItemId.h` | 新增 `Weed` |
| W-G | `AWeedEntity.h`（新建） | 新 UCLASS |

W-B（cpp 部分）/ W-C / W-D / W-E 全部為純 cpp 修改，可 Live Coding。
