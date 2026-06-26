# 材質物理屬性擴充計畫

> 參考：Noita `CellData.xml` 屬性清單、Minecraft Block Properties Wiki
> 建立日期：2026-06-25
> 狀態：Phase 1 ✅ | Phase 2~4 📋 計畫中

---

## 背景

目前 `FMaterialData` 的物理欄位偏向 CA 模擬最小集合（Physics、Density 只用於液體/粉末排擠）。
本計畫在不改動現有 CA 邏輯的前提下，依序擴充以下三類屬性：

1. **密度全材質化** — 已有欄位，補全所有材質的數值
2. **質量系統** — 生物 / 物品 / 放置物增加 `Mass`，用於衝擊力計算
3. **16 項新材質屬性** — 仿 Noita / Minecraft 補完物理 / 化學 / 光照 / 行為欄位

---

## 一、密度全材質化

### 現況
`FMaterialData.Density` 已存在，但 Static/Gas 材質預設 0.f，只有 Liquid/Powder 有語意值。

### 目標
所有材質補上密度（以水=1.0 為基準的相對值），供以下用途：
- 粉末 / 液體排擠判斷（現有）
- 掉落衝擊力 = `Density × TileVolumeCm³ × v²/2`
- 爆炸飛散動能計算

### 參考值（單位：相對密度，水=1.0）

| 材質 | 現值 | 目標值 | 參考 |
|------|------|--------|------|
| Air | 0 | 0 | — |
| Stone | 0 | 2.5 | 現實花崗岩 2.6 |
| Dirt | 0 | 1.3 | 潮濕土壤 |
| Dirt_Dry | 0 | 0.9 | 乾燥土壤 |
| Sand | 6.0 | 1.6 | Godot 原值（用於液體排擠），但作為絕對密度偏高，需統一比例尺後調整 |
| Wood | 0 | 0.6 | 松木 0.5~0.7 |
| Ash | 0 | 0.3 | 木灰 |
| Water | 1.0 | 1.0 | 基準 |
| Lava | 3.0 | 3.0 | 玄武岩岩漿 |
| Coal | 0 | 1.4 | 煤炭 |
| Copper | 0 | 8.9 | 純銅 |
| Iron | 0 | 7.9 | 純鐵 |
| MagicCrystal | 0 | 2.8 | 自訂 |
| Grass | 0 | 1.0 | 近似潮濕土 |
| Fire | 0 | 0.001 | 氣體，近似無質量 |
| Steam | 0 | 0.0006 | 水蒸氣 |

> ⚠️ Sand 的 Density=6.0f 是在「液體排擠」比例下的值，若改成現實相對密度須同步調整 GPU/CPU CA 排擠判斷的比對邏輯。
> 建議保持兩套數值：`Density`（CA 排擠用，現有）+ `RealDensity`（物理計算用，新增）；或統一縮放。

### 待決策
- [ ] 採「單一 Density + 縮放係數」還是「拆成 CaDensity + PhysicsDensity 兩欄位」？

---

## 二、質量系統（Mass）

### 設計原則
- 使用 **質量（Mass）** 而非重量（Weight），因為重力場未來可能被 spell/地形改變
- `Weight = Mass × g`，g 可變；Mass 不變
- 衝擊力公式：`KineticEnergy = 0.5 × Mass × v²`，`ImpactForce = Mass × Δv / Δt`

### 需要加 Mass 的地方

#### 2-A. `FCharacterStats`（生物）
```cpp
// 角色質量（kg 等比單位）；用於墜落傷害、爆炸飛散、水中浮沉
float Mass = 70.f;  // 人型預設 70
```

預設參考值：

| 生物類型 | Mass | 備註 |
|---------|------|------|
| 玩家 | 70.f | 可由種族/體型修正 |
| NPC（人型） | 60.f | |
| Melee Beast | 80.f | |
| Ranged Beast | 50.f | |
| Patrol Beast | 90.f | |
| Heavy Beast | 200.f | |

#### 2-B. `FItemData`（物品）
```cpp
// 物品質量（kg 等比單位）；投擲命中傷害 = 0.5 × Mass × v²
float Mass = 0.1f;  // 預設 100g
```

#### 2-C. 放置物 / Fixture（選用）
放置物通常靜止，暫時從 `FItemData.Mass` 繼承即可，不另存。

### 使用路徑
- **墜落傷害**：`ASkillCreatorCharacter` 落地時 `v = sqrt(2g×h)`，傷害 ∝ `Mass × v²`
- **爆炸飛散**：爆炸力 / Mass = 飛散加速度（Mass 越大越難移動）
- **投擲命中**：`ASpellProjectile` 命中時 `KE = 0.5 × ItemMass × projSpeed²`
- **液體浮沉**：`Mass / (Density × TileVolumeM³) < 1.0` → 上浮（未來水中物理用）

---

## 三、16 項新材質屬性

依優先級分三組，全部加進 `FMaterialData`：

---

### 3-A 🔴 高優先（直接接入現有元素 / CA 系統）

#### P-1. AutoignitionTemp（自燃溫度）
```cpp
float AutoignitionTemp = -1.f;
// -1 = 不會自燃；> 0 = 達到此溫度（°C）後自動點燃
// 用於：熔岩旁木材自燃、Fire 元素持續加熱效果
```

| 材質 | 建議值 |
|------|--------|
| Wood | 250.f |
| Ash | -1.f（灰燼不再燃） |
| Coal | 400.f |
| Grass | 180.f |
| 其餘 | -1.f |

**接入點**：`CheckElementalCaReactions()` 或新增 `CheckTemperatureEffects()` 每 N 幀掃周圍材質溫度。

#### P-2. MeltToMaterial（加熱轉換）
```cpp
EMaterialType MeltToMaterial = EMaterialType::Air;
// Air = 無轉換；否則加熱後變成此材質
```

| 材質 | MeltToMaterial |
|------|----------------|
| Ice（待新增） | Water |
| Stone | Lava |
| Iron | Lava（需要極高溫） |
| 其餘 | Air |

**接入點**：`ApplyElementalImpact(Fire)` + 溫度累積超閾值時呼叫 `SetTile(MeltToMaterial)`。

#### P-3. FreezeToMaterial（冷凍轉換）
```cpp
EMaterialType FreezeToMaterial = EMaterialType::Air;
// Air = 無轉換；Ice 元素命中後變成此材質
```

| 材質 | FreezeToMaterial |
|------|-----------------|
| Water | Ice（待新增） |
| Lava | Stone |
| Steam | Water |
| 其餘 | Air |

**接入點**：`ApplyElementalImpact(Ice)` 時讀取此欄位。

#### P-4. ElectricalConductivity（導電度）
```cpp
float ElectricalConductivity = 0.f;
// 0 = 絕緣；1 = 完全導電
// 用於 Thunder 元素感電傳播：沿鄰接導體格連鎖
```

| 材質 | 建議值 |
|------|--------|
| Water | 0.8f |
| Iron | 1.0f |
| Copper | 1.0f |
| Stone | 0.05f |
| Wood | 0.01f |
| Air/Fire/Steam | 0.f |

**接入點**：`ApplyElementalImpact(Thunder)` 後新增 `PropagateThunder()` 廣度優先搜尋鄰格 conductivity > 閾值。

#### P-5. LuminanceLevel（自發光等級）
```cpp
uint8 LuminanceLevel = 0;
// 0 = 無發光；15 = 最亮（參考 MC 等級制）
// 用於：動態點光源、視野照明
```

| 材質 | LuminanceLevel |
|------|---------------|
| Fire | 13 |
| Lava | 15 |
| MagicCrystal | 8 |
| Steam | 0 |
| 其餘 | 0 |

**接入點**：`AVoxelWorldActor` Chunk 渲染時，對 LuminanceLevel > 0 的格子加 `UPointLightComponent`（或 RMC 自發光材質）。

---

### 3-B 🟡 中優先（CA 物理細化）

#### P-6. LiquidFlowSpeed（液體流速）
```cpp
float LiquidFlowSpeed = 1.0f;
// 0.0 = 不流動；1.0 = 正常水速；< 1 = 黏稠
// 只對 EPhysicsCategory::Liquid 有效
```

| 材質 | LiquidFlowSpeed |
|------|----------------|
| Water | 1.0f |
| Lava | 0.2f |

**接入點**：CPU `UpdateLiquid()` 決定每次最多擴散幾格；GPU shader 以此決定 liquid_damping。

#### P-7. LiquidViscosity（液體黏度）
```cpp
float LiquidViscosity = 0.f;
// 0 = 無黏性；1 = 極黏（踩進去移速受影響）
// 同時影響液體向兩側擴散的阻力
```

| 材質 | LiquidViscosity |
|------|----------------|
| Water | 0.f |
| Lava | 0.9f |

#### P-8 / P-9. GasUpwardSpeed / GasHorizontalSpeed（氣體速度向量）
```cpp
float GasUpwardSpeed     = 1.0f;  // 上浮速度係數（0-2）
float GasHorizontalSpeed = 1.0f;  // 橫向擴散係數（0-2）
// 只對 EPhysicsCategory::Gas 有效
```

| 材質 | GasUpwardSpeed | GasHorizontalSpeed |
|------|---------------|-------------------|
| Fire | 1.5f | 0.8f |
| Steam | 2.0f | 0.6f |

#### P-10. GasLifetime（氣體壽命）
```cpp
uint16 GasLifetime = 0;
// 0 = 不自動消散（靠 BurnMax 控制）
// > 0 = 存在最多 N tick 後轉成 Air
```

| 材質 | GasLifetime |
|------|-------------|
| Fire | 0（靠 BurnMax） |
| Steam | 240（≈4秒@60fps） |

#### P-11. BreakToMaterial（破壞後轉換）
```cpp
EMaterialType BreakToMaterial = EMaterialType::Air;
// 採礦 / 爆炸破壞後，原格變成什麼材質（掉落物另算）
// Air = 直接清空（現有行為）
```

| 材質 | BreakToMaterial |
|------|----------------|
| Wood | Ash（燒過的木頭碎裂留灰） |
| 其餘 | Air |

---

### 3-C 🟢 低優先（行為 / 互動細節）

#### P-12. ContactStatusEffect（接觸狀態效果）
```cpp
FName ContactStatusEffect = NAME_None;
// 踩進此材質格時，自動對玩家 AuraComp 施加的效果 tag
// 例："Slow" "Poison" "Wet"
```

| 材質 | ContactStatusEffect |
|------|---------------------|
| Lava | "Burning" |
| Water | "Wet"（未實作） |
| 其餘 | NAME_None |

#### P-13. SpeedFactor（站立移速乘數）
```cpp
float SpeedFactor = 1.0f;
// 站在此材質上方時的水平移速乘數（乘以角色基礎走速）
// 1.0 = 正常；< 1 = 減速（Soul Sand、蜂蜜表面）；> 1 = 加速（未來魔法地板）
// 與 Stickyness 區別：SpeedFactor 作用於站立表面（乘法）；Stickyness 作用於浸入液體（阻力）
```

| 材質 | SpeedFactor | 參考來源 |
|------|-------------|---------|
| 正常地面 | 1.0f | MC 預設 |
| Lava（表面） | 0.f（不可站） | — |
| Ice（待新增） | 1.0f（冰面靠 Slippery 控制，非減速） | MC |
| 其餘 | 1.0f | — |

#### P-14. Stickyness（浸入黏滯度）
```cpp
float Stickyness = 0.f;
// 浸入此材質（液體/泥漿）時的移速阻尼：最終速度 × (1 - Stickyness)
// 0 = 無阻力；1 = 完全卡死
// 與 SpeedFactor 區別：Stickyness 作用於浸入（液體接觸）；SpeedFactor 作用於站立表面
```

| 材質 | Stickyness |
|------|-----------|
| Water | 0.3f（水中移動減速） |
| Lava | 0.95f（幾乎無法在熔岩中移動） |
| 其餘 | 0.f |

#### P-15. Slippery（表面摩擦係數）
```cpp
float Slippery = 0.f;
// 0 = 正常摩擦（急停即停）；1 = 完全無摩擦（冰面永久滑行）
// 影響 ASkillCreatorCharacter 的地面摩擦力 → 決定加減速曲線
// MC 對應：friction（預設 0.6；Ice = 0.98；Blue Ice = 0.989）
```

| 材質 | Slippery | 等效 MC friction |
|------|---------|----------------|
| 一般地面 | 0.f | 0.6（預設） |
| Ice（待新增） | 0.85f | 0.98 |
| Blue Ice（待新增） | 0.95f | 0.989 |
| 其餘 | 0.f | 0.6 |

#### P-16. Restitution（彈性碰撞係數）
```cpp
float Restitution = 0.f;
// 實體落地時的速度反彈比例：反彈速度 = 入射速度 × Restitution
// 0 = 完全吸收（普通地面）；1 = 完全彈性碰撞（理論值）
// MC Slime Block 行為：從 255 格落下彈回 ≈50 格 → 約 0.44；從 50 格彈回 22 格 → 約 0.66
//   Noita solid_restitution 最高值 0.7
// 注意：UE5 物理引擎本身有 Restitution，但 tile 格是靜態 CA 而非物理 Actor，
//   需要在 ASkillCreatorCharacter 落地事件中手動讀取腳下材質的 Restitution 計算
```

| 材質 | Restitution | 說明 |
|------|-------------|------|
| 一般地面 | 0.f | 完全吸收 |
| Grass | 0.05f | 些微緩衝 |
| Dirt_Dry | 0.02f | |
| MagicCrystal | 0.3f | 自訂（彈跳感） |

#### P-17. JumpFactor（跳躍高度乘數）
```cpp
float JumpFactor = 1.0f;
// 站在此材質上跳躍時，初速度的乘數：JumpVelocity × JumpFactor
// 1.0 = 正常；< 1 = 壓制跳躍（Honey Block ≈ 0.4）；> 1 = 彈跳增強（蘑菇/彈簧地板）
// MC Honey Block：只能跳 3/16 格（正常 1.25 格）→ 高度比 ≈ 0.15 → 速度比 sqrt(0.15) ≈ 0.39
// 注意：JumpFactor 作用於初速度；Restitution 作用於落地後反彈，兩者獨立
```

| 材質 | JumpFactor | 說明 |
|------|-----------|------|
| 一般地面 | 1.0f | 正常跳躍 |
| Lava（表面） | 0.f（不可跳） | — |

#### P-18. PlatformType（平台類型）
```cpp
uint8 PlatformType = 0;
// 0 = 正常實心（可站）
// 1 = 可穿越（Sand 崩塌前）
// 2 = 單向平台（從下方可穿過）
```

#### P-19. DangerFlags（AI 危險旗標）
```cpp
uint8 DangerFlags = 0;
// bit 0 = bDangerFire（NPC 迴避）
// bit 1 = bDangerWater
// bit 2 = bDangerPoison
// bit 3 = bDangerRadioactive
```

| 材質 | DangerFlags |
|------|-------------|
| Fire | 0b0001（Fire） |
| Lava | 0b0001（Fire） |
| 其餘 | 0 |

---

## 四、實作順序建議

### Phase 1 ✅ — 純資料（不動 CA 邏輯）
只加欄位 + 填值，Build 0 錯誤即可：

1. [x] `FMaterialData` 加入全部 19 個欄位（預設值安全）
2. [x] `FCharacterStats` 加 `Mass`
3. [x] `FItemData` 加 `Mass`
4. [x] `MaterialRegistry.cpp` 補全所有材質的數值

> 這一步純粹是資料結構，不改任何執行時邏輯，風險最低。

### Phase 2 ✅ — 接入高優先邏輯（P-1 ~ P-5）
5. [x] `FreezeToMaterial`：接入 `ApplyElementalImpact(Ice)`
6. [x] `MeltToMaterial`：接入 `ApplyElementalImpact(Fire)`
7. [x] `LuminanceLevel`：`RebuildChunkLights(MC)` — 每 16³ tile 子區域 1 個 UPointLightComponent，強度/半徑由 MaxLum/15 決定，NativeElement 決定顏色（Fire=暖橙/Light=冷藍/其餘=暖白），不投陰影；`ClearAllChunkLights()` 於 `ReinitializeForWorld` 清除
8. [x] `ElectricalConductivity`：`PropagateThunder()` BFS（Threshold=0.3, MaxSteps=20）
9. [x] `AutoignitionTemp`：`IgniteMaterial` + `TryIgniteAround` 加 AutoignitionTemp 材質支援（AutoignitionTemp≥0 可被引燃；機率 = Chance × max(1, 300/AutoignitionTemp)）

### Phase 3 ✅ — CA 物理細化（P-6 ~ P-11）
10. [x] P-6/P-7 液體流速/黏度：`UpdateLiquid()` 改資料驅動（SkipN=round(1/FlowSpeed)，Spread=FlowSpeed×3×(1-Viscosity)），Lava FlowSpeed 由 0.2→0.333 維持 Godot Frame%3 行為
11. [x] P-8/P-9 氣體速度向量：`UpdateGas()` 上浮機率=min(1,GasUpwardSpeed)，斜向機率=min(1,GasHorizontalSpeed)
12. [x] P-10 GasLifetime：`SetTile()` 在 InitState=0 且 Gas.GasLifetime>0 時自動設 CA_State
13. [x] P-11 BreakToMaterial：`DestroyTile()` 破壞後轉換為 BreakToMaterial（非 Air 時）

### Phase 4 ✅ — 行為互動（P-12 ~ P-19）
14. [x] P-12 ContactEffect：`ApplyEnvironmentalDamage()` 映射 EContactEffect→ESkillElementType，呼叫 AuraComp->Apply()（1 秒冷卻避免每幀疊加）
15. [x] P-13/P-14/P-15 SpeedFactor/Stickyness/Slippery：`ApplyEnvironmentalDamage()` 每幀依移動狀態 BaseSpeed × SpeedFactor(腳下) × (1−Stickyness)(身體格) 更新 MaxWalkSpeed；GroundFriction = 8×(1−Slippery)（非飛行時生效）
16. [x] P-16 Restitution：`Landed()` 在 Super 前讀取 Velocity.Z，落地後依腳下材質 Restitution LaunchCharacter 向上反彈
17. [x] P-17 JumpFactor：`OnJumpStarted()` 臨時縮放 JumpZVelocity×JumpFactor，Jump() 後立刻還原（不影響空中物理）
18. [x] P-18 PlatformType=1：`RMCPassthroughComp`（`ECollisionEnabled::NoCollision`）+ `EGreedyMeshFilter::PassthroughOnly` 獨立渲染 passthrough tile；PlatformType=2 在 3D 語意不明確，永久略過
19. [x] P-19 DangerFlags：`ANPCAIController::TryStep()` 攔截 DangerFlags!=0 的目標格地板，NPC 直接跳過不踏入危險格

---

## 五、相關檔案

| 檔案 | 異動 |
|------|------|
| `Plugins/VoxelWorld/Source/VoxelWorld/Public/MaterialRegistry.h` | 加 19 欄位 + Density 補值 |
| `Plugins/VoxelWorld/Source/VoxelWorld/Private/MaterialRegistry.cpp` | 所有材質補值 |
| `Source/SkillCreatorCore/Public/CharacterStats.h` | 加 `Mass` |
| `Source/SkillCreatorCore/Public/ItemData.h` | 加 `Mass` |
| `Plugins/VoxelWorld/Source/VoxelWorld/Private/TileWorld3D.cpp` | Phase 2~4 各接入點 |
| `Source/SkillCreatorRuntime/Private/ASkillCreatorCharacter.cpp` | 墜落傷害公式 + Slippery |
| `Source/SkillCreatorRuntime/Private/ABeastCharacter.cpp` | 爆炸飛散用 Mass |

---

## 六、暫不處理

- `generates_smoke / generates_flames`（視覺粒子，待美術資產確認）
- 植被屬性（另有 W-A 樹木計畫）
- 音訊材質分類（待音效資產）
- 液體染色（`liquid_stains`）
