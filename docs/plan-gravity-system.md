# 重力系統計畫

> 建立日期：2026-06-25
> 狀態：📋 計畫中
> 關聯：`plan-material-physics.md`（GravityScale 影響 Mass / JumpFactor / Restitution 計算）

---

## ⚠️ 閱讀提醒

**當你要設計「實體物品（Physical Item）」時，先回來看這份文件。**

實體物品 = 能掉落在地上、被撿起、被踢飛、浮在液體上的世界物件，
其物理行為（落下速度、彈跳、漂浮）全部由本計畫的重力場決定。
在動手做實體物品之前，需要先確認本計畫的 Phase 1（GravityScale）已實作完成。

---

## 一、核心概念

### 重力場的兩個維度

| 維度 | 描述 | 支援計畫 |
|------|------|---------|
| **大小（Scale）** | g 的倍率，影響落速、跳高、衝擊力 | Phase 1（本計畫） |
| **方向（Direction）** | 向下軸改變（反重力、側重力） | Phase 3（長期，需 CA 大改） |

---

## 二、Phase 1 — GravityScale（大小）

### 資料結構

```cpp
// WorldScale.h 新增
namespace WorldScale
{
    // 全局重力倍率（1.0 = 正常；0.5 = 月球；2.0 = 重力異常區）
    // 未來擴充：per-region，見 FGravityZone
    inline float GlobalGravityScale = 1.0f;
}
```

未來 per-region 版本預留：

```cpp
struct FGravityZone
{
    FBox   Region;         // 世界座標 AABB
    float  Scale = 1.0f;  // 此區域重力倍率
    // Phase 3：FVector Direction = FVector(0, 1, 0);  // UE5 Y 向下
};
// AVoxelWorldActor 持有 TArray<FGravityZone> GravityZones;
```

### 對各系統的影響

#### 2-A. CA（Powder / Liquid / Gas）

```cpp
// TileWorld3D.cpp — UpdatePowder() / UpdateLiquid() / UpdateGas()
// 大小影響：每 tick 嘗試落下的格數
int32 FallSteps = FMath::Max(1, FMath::RoundToInt(WorldScale::GlobalGravityScale));
for (int32 i = 0; i < FallSteps; ++i)
    if (!TryFallOneStep(x, y, z)) break;

// 低重力（Scale < 1）：用機率控制
// Scale = 0.5 → 每 tick 50% 機率嘗試落下
if (FMath::FRand() < WorldScale::GlobalGravityScale)
    TryFallOneStep(x, y, z);
```

#### 2-B. 生物 / 玩家（Entity）

```cpp
// ASkillCreatorCharacter / ABeastCharacter
// 跳躍初速（高度 ∝ v²/2g → 維持同跳高需 v ∝ sqrt(g)）
const float g = WorldScale::GlobalGravityScale;
const float JumpV = BaseJumpVelocity * FMath::Sqrt(g);
GetCharacterMovement()->GravityScale = g;

// 墜落傷害（KE = ½mv²，v² = 2gh）
float FallDamage = 0.5f * Stats.Mass * 2.f * GravityCmSec2 * g * FallHeightCm;
```

#### 2-C. 實體物品（Physical Item）⚠️ 待實作

> **本節是為「實體物品」系統預留的接口說明。
> 實體物品尚未設計，但其物理行為將高度依賴本節的公式與 GravityZone。**

預想行為：
- 實體物品掉落速度 = `v(t) = GlobalGravityScale × g × t`
- 落地彈跳 = `v_bounce = v_impact × FMaterialData::Restitution`（腳下材質）
- 液體浮沉 = `Mass < FluidDensity × TileVolumeCm³` → 上浮（使用 FMaterialData::Density）
- 被爆炸飛散 = `Acceleration = ExplosionForce / Mass`（Mass 越大越難移動）
- 在重力場中漂浮 = `GlobalGravityScale = 0` → 實體物品懸停在空中

實體物品 Actor 設計提示（實作時參考）：
```cpp
// APhysicalItemActor（未來新增）
class APhysicalItemActor : public AActor
{
    EItemId      ItemId;
    float        Mass;         // 從 FItemRegistry::Get(ItemId).Mass 初始化
    FVector      Velocity;     // 手動積分，不用 UE5 Physics（配合 Tile 格解析度）
    FIntVector   GridPos;      // 當前所在 Tile 格

    void Tick(float DeltaTime)
    {
        const float g = WorldScale::GlobalGravityScale;
        Velocity.Y += GravityCmSec2 * g * DeltaTime;   // UE5 Y 向下
        // 嘗試移動到新格，碰到非 Air → 落地事件 → 計算 Restitution 反彈
    }
};
```

---

## 三、Phase 2 — per-Region GravityScale（區域重力）

比全局 GravityScale 更精細：不同地形區域有不同的重力強度。

實作要點：
- `AVoxelWorldActor` 持有 `TArray<FGravityZone>`
- CA 每格 update 時查詢所在座標屬於哪個 GravityZone（O(n) 線性掃，n = zone 數量，通常極少）
- Entity 的 `CharacterMovementComponent->GravityScale` 在進入 / 離開 zone 時動態更新
- 實體物品 tick 時查詢自身格的 GravityZone

觸發方式：
- Spell 效果（「重力領域」積木）→ 生成暫時 FGravityZone
- 地形特徵（地下空洞、天空島）→ 靜態 FGravityZone 在 World Init 時注入

---

## 四、Phase 3 — 方向改變（長期）

> **此 Phase 需要大幅修改 CA 更新邏輯，列為長期目標，不急。**

現況：CA 的「向下」是硬編碼 Y+1，沒有重力向量。

目標：把 CA update 的「向下」抽象成 `FIntVector GravityDir = {0, 1, 0}`（可為任意軸）。

```cpp
struct FGravityZone
{
    FBox      Region;
    float     Scale     = 1.0f;
    FIntVector Direction = {0, 1, 0};  // Y+ = 向下（UE5 座標）
};
```

主要改動：
- `UpdatePowder(x,y,z)`：把 `y+1` 全部改成 `y + GravityDir.Y`，`x + GravityDir.X`，`z + GravityDir.Z`
- `UpdateLiquid()`：橫向擴散方向改成與 GravityDir 垂直的平面
- `UpdateGas()`：上浮方向改成 `-GravityDir`
- Entity：`GravityVector = FVector(GravityDir) * GravityScale * GravityCmSec2`

**為什麼重力只影響 Entity 而不影響 CA 會很怪：**
玩家站在反重力區飄起來，腳下的水仍往 Y+ 流、火仍往 Y- 竄，物理感完全矛盾。
方向改變必須 CA + Entity 同步，否則不如不做。

---

## 五、與其他系統的關聯

| 系統 | 關聯點 |
|------|--------|
| `plan-material-physics.md` | `Mass`（衝擊力）、`Restitution`（反彈）、`JumpFactor`（跳高）、`Density`（浮沉） |
| `plan-ability-system.md` | 「重力領域」spell 效果 → 生成暫時 FGravityZone |
| `plan-placed-objects.md` | 放置物的靜態穩定性（重力改向後是否倒塌？） |
| **實體物品（尚未設計）** | 整套落地 / 反彈 / 浮沉 / 飛散公式都從本計畫繼承 |

---

## 六、實作順序

- [x] Phase 1-A：`WorldScale.h` 加 `GlobalGravityScale`（由 plan-physical-item.md G-1 完成，2026-06-26）
- [ ] Phase 1-B：CA `UpdatePowder/Liquid/Gas` 讀取 `GlobalGravityScale`（與實體物品無關，獨立排期）
- [ ] Phase 1-C：Entity Jump / GravityScale / 墜落傷害接入（獨立排期）
- [x] Phase 1-D：**實體物品 `APhysicalItemActor` 接入**（由 plan-physical-item.md G-3 完成，2026-06-26）
- [ ] Phase 2：FGravityZone per-region 支援（需技能系統「重力領域」積木）
- [ ] Phase 3：CA 方向向量抽象化（長期）
