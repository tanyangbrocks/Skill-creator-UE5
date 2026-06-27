# APhysicalItemActor → Chaos Physics 遷移計畫

> 撰寫日期：2026-06-27
> 依據：`docs/plugin-assessment.html` §「實體物品投擲」+ 現有 `APhysicalItemActor` 原始碼分析
> 目標：以 Chaos Physics 取代手寫 `PhysicsTick()`，刪除約 150 行手寫物理代碼，
>        自動獲得旋轉／滾動動畫，與引擎 physics 管線整合。

---

## 一、現況診斷

`APhysicalItemActor` 目前：

| 手寫物理函數 | 行數 | 說明 |
|------------|------|------|
| `PhysicsTick()` | ~30 | 重力積分、位移、液體浮力 |
| `HandleTileCollision()` | ~15 | Restitution 反彈、摩擦、落地判定 |
| `IsSolidAt()` | ~8 | 查詢 tile 是否為實心 |
| `GetBelowRestitution()` | ~10 | 查 FMaterialRegistry 取反彈係數 |

`MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision)` — 完全不參與 Chaos。

**`ADebrisActor` 已確認**：對 RMC mesh 做 `SetSimulatePhysics(true)` 可正常碰撞，
證明 Chaos 看得到 CA 世界幾何體（`plugin-assessment.html` v2 修正）。

---

## 二、不可替換的行為（需保留）

| 行為 | 現況實作 | Chaos 遷移後 |
|------|---------|-------------|
| **Tile 材質 Restitution** | `GetBelowRestitution()` 查 `FMaterialRegistry` | 建立 `UPhysicalMaterial` 資產，RMC section 指定 Physical Material → Chaos 自動套用 |
| **液體浮力** | `PhysicsTick()` 查 tile 下方材質類別 | `Tick()` 查同一份材質，呼叫 `MeshComp->AddForce()` |
| **攜帶跟隨角色** | `Tick()` 設 `SetActorLocation()` | `SetSimulatePhysics(false)` + `SetActorLocation()` 不變 |

---

## 三、架構決策

### Restitution 方案

**方案 A（本計畫採用）**  
為每個 `EMaterialType` 建立 `UPhysicalMaterial` 資產，在 RMC GreedyMesher 提交 section 時
依 tile 材質指定 Physical Material。Chaos 碰撞時自動套用正確係數，不需任何回調程式碼。

**方案 B（備選，若 RMC API 不支援 per-section Physical Material）**  
`OnComponentHit` 回調中查詢命中點 tile 材質，手動計算修正衝量。行為正確但程式碼複雜，
僅在 PHYS-2 遇到阻礙時退回此方案。

### 液體浮力方案

Chaos 不感知 tile 材質，**不使用靜態 trigger Actor**（液面是動態 CA 世界，靜態 trigger 無法跟上）。  
在 `Tick()` 查詢物品下方 tile 的 `EPhysicsCategory`，若為 `Liquid` 則呼叫 `AddForce()` 抵消重力。

---

## 四、分步實作

### PHYS-0：建立 `UPhysicalMaterial` 資產（Python 腳本，無 C++ 改動）

**目標**：在 `/Game/PhysicalMaterials/` 為各主要材質建立 `.uasset`，設 Restitution 與 Friction。

建立 Python 腳本 `generate_physical_materials.py`（在 UE5 Editor Python Console 執行）：

```python
import unreal

MATERIALS = {
    # 格式：AssetName: (Restitution, Friction)
    "PM_Stone":      (0.35, 0.70),
    "PM_Dirt":       (0.10, 0.60),
    "PM_Sand":       (0.05, 0.50),
    "PM_Gravel":     (0.20, 0.65),
    "PM_Wood":       (0.25, 0.55),
    "PM_Metal":      (0.50, 0.40),
    "PM_Grass":      (0.10, 0.70),
    "PM_Ice":        (0.45, 0.05),
    "PM_Lava":       (0.05, 0.80),
    "PM_Water":      (0.00, 0.10),  # 液體用浮力取代，Restitution 不重要
    "PM_Default":    (0.20, 0.60),  # 未對應材質的 fallback
}

factory = unreal.PhysicalMaterialFactory()
al = unreal.AssetLibrary if hasattr(unreal, 'AssetLibrary') else unreal.EditorAssetLibrary

for name, (restitution, friction) in MATERIALS.items():
    path = f"/Game/PhysicalMaterials/{name}"
    if al.does_asset_exist(path):
        pm = unreal.load_asset(path)
    else:
        pm = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, "/Game/PhysicalMaterials",
            unreal.PhysicalMaterial, factory)
    if pm:
        pm.set_editor_property("restitution", restitution)
        pm.set_editor_property("friction", friction)
        unreal.EditorAssetLibrary.save_asset(path)
        print(f"  ✓ {name}  Restitution={restitution}  Friction={friction}")
```

**執行**：Output Log → Cmd → `py "C:\SkillCreatorUE5\generate_physical_materials.py"`  
**無 C++ 改動，不需 Rebuild。**

---

### PHYS-1：`FMaterialData` 加 Physical Material 軟參照（需 Rebuild）

**目標**：`FMaterialRegistry` 可查出每種材質對應的 Physical Material 資產路徑。

**修改 `Plugins/VoxelWorld/Source/.../MaterialRegistry.h`**（或對應位置）：

```cpp
struct FMaterialData
{
    // ... 既有欄位 ...
    FSoftObjectPath PhysicalMaterialPath;  // 新增：/Game/PhysicalMaterials/PM_{Name}
};
```

**修改 `MaterialRegistry.cpp`** — 在各 `Register()` 呼叫填入路徑：

```cpp
// 範例（依實際 EMaterialType 枚舉值補全）
{ auto& D = Register(EMaterialType::Stone);
  D.PhysicalMaterialPath = FSoftObjectPath(TEXT("/Game/PhysicalMaterials/PM_Stone.PM_Stone")); }
{ auto& D = Register(EMaterialType::Dirt);
  D.PhysicalMaterialPath = FSoftObjectPath(TEXT("/Game/PhysicalMaterials/PM_Dirt.PM_Dirt")); }
// ... 依此類推 ...
```

**加輔助函數**（MaterialRegistry.h）：

```cpp
static UPhysicalMaterial* GetPhysicalMaterial(EMaterialType Mat);
```

```cpp
// MaterialRegistry.cpp
UPhysicalMaterial* FMaterialRegistry::GetPhysicalMaterial(EMaterialType Mat)
{
    const FSoftObjectPath& Path = Get(Mat).PhysicalMaterialPath;
    if (Path.IsNull()) return nullptr;
    return Cast<UPhysicalMaterial>(Path.TryLoad());
}
```

**`.h` 有變動 → 必須關 Editor + 完整 Rebuild。**

---

### PHYS-2：RMC GreedyMesher 指定 Physical Material（可 Live Coding）

**目標**：RMC mesh section 依 tile 材質帶上 `UPhysicalMaterial`，Chaos 碰撞時自動套用正確 Restitution。

找到 GreedyMesher 提交 section 的位置（搜尋 `CreateMeshSection` 或 `SetupMaterialSlot` 等 RMC API 呼叫），
在 section 設定中加上 Physical Material：

```cpp
// 偽代碼，依實際 RMC API 版本調整
UStaticMesh* SectionMesh = ...; // 或 RMC section 設定
EMaterialType TileMat = ...; // 本次 section 的材質類型
if (UPhysicalMaterial* PM = FMaterialRegistry::GetPhysicalMaterial(TileMat))
{
    // RMC 5.x API 方式（依版本不同）：
    // 方式 A：透過 FRealtimeMeshCollisionConfiguration 指定 Physical Material
    // 方式 B：在 UMaterialInterface 的 Physical Material 欄位設定
    // 若 RMC 不支援 per-section Physical Material → 退回方案 B（OnComponentHit callback）
}
```

**⚠️ 若 RMC API 不支援**：跳過本步驟，改在 PHYS-3 的 `OnComponentHit` 中手動查詢修正衝量（方案 B）。

**純 `.cpp` 改動，可 Live Coding。**

---

### PHYS-3：`APhysicalItemActor` Chaos 遷移（需 Rebuild）

**目標**：移除所有手寫物理，改由 Chaos 接管，保留攜帶邏輯。

#### 3-A：`.h` 修改

**移除**：
```cpp
// 刪除以下私有宣告
FVector Velocity;
bool    bLanded;
void PhysicsTick(float DeltaTime);
void HandleTileCollision(int32 VoxX, int32 VoxY, int32 VoxZ);
bool IsSolidAt(int32 VoxX, int32 VoxY, int32 VoxZ) const;
float GetBelowRestitution() const;
```

**新增**：
```cpp
// 液體浮力狀態（Tick 用）
bool bIsInLiquid = false;

// OnHit 回調（方案 B 備用，RMC API 不支援 per-section PM 時才接線）
UFUNCTION()
void OnItemHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
               UPrimitiveComponent* OtherComp, FVector NormalImpulse,
               const FHitResult& Hit);
```

#### 3-B：`constructor` 修改

```cpp
APhysicalItemActor::APhysicalItemActor()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    // ← 改為有效碰撞 + 啟用 Chaos 模擬
    MeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
    MeshComp->SetSimulatePhysics(false);  // Init() 完成後才啟用
    MeshComp->SetNotifyRigidBodyCollision(true);  // 觸發 OnHit 事件
    MeshComp->SetEnableGravity(true);
    RootComponent = MeshComp;

    PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
    PickupSphere->SetupAttachment(RootComponent);
    PickupSphere->SetSphereRadius(WorldScale::TileSizeCm * 3.f);
    PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PickupSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
}
```

#### 3-C：`Init()` 修改

```cpp
void APhysicalItemActor::Init(EItemId InItemId, int32 InCount, FVector InitialVelocityCms)
{
    ItemId = InItemId;
    Count  = InCount;

    // ... 三層 Mesh fallback 不變 ...

    // 依物品重量設定 Chaos mass（覆蓋 Mesh 預設質量）
    const FItemData& Data = FItemRegistry::Get(InItemId);
    MeshComp->SetMassOverrideInKg(NAME_None, Data.Mass, true);

    // 初速：有投擲速度才啟用物理 + 加衝量
    if (!InitialVelocityCms.IsZero())
    {
        MeshComp->SetSimulatePhysics(true);
        MeshComp->AddImpulse(InitialVelocityCms);  // cm/s * kg = impulse
    }
}
```

#### 3-D：`OnCarried()` / `OnReleased()` 修改

```cpp
void APhysicalItemActor::OnCarried(ASkillCreatorCharacter* InCarrier)
{
    bBeingCarried = true;
    Carrier = InCarrier;
    MeshComp->SetSimulatePhysics(false);
    MeshComp->SetEnableGravity(false);
    SetActorEnableCollision(false);
}

void APhysicalItemActor::OnReleased(FVector ThrowVelocityCms)
{
    bBeingCarried = false;
    Carrier = nullptr;
    SetActorEnableCollision(true);
    MeshComp->SetEnableGravity(true);
    MeshComp->SetSimulatePhysics(true);
    if (!ThrowVelocityCms.IsZero())
    {
        const float Mass = MeshComp->GetMass();  // Chaos 計算的實際質量
        MeshComp->AddImpulse(ThrowVelocityCms * Mass);
    }
}
```

#### 3-E：`Tick()` 修改

```cpp
void APhysicalItemActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bBeingCarried)
    {
        // 攜帶跟隨邏輯不變
        if (Carrier.IsValid())
        {
            FVector HoldPos = Carrier->GetActorLocation()
                + Carrier->GetActorForwardVector() * WorldScale::TileSizeCm * 2.f
                + FVector(0.f, 0.f, WorldScale::TileSizeCm * 1.f);
            SetActorLocation(HoldPos);
        }
        return;
    }

    // PHYS-4 液體浮力（見下一步）
}
```

**`.h` 有變動 → 必須關 Editor + 完整 Rebuild。**

---

### PHYS-4：液體浮力接入（可 Live Coding）

**目標**：物品落入液體格時獲得向上浮力，不沉底。

在 `Tick()` 的 `return` 之後（非攜帶中）加入：

```cpp
// 液體浮力：Chaos 不感知 tile 材質，手動施加
if (MeshComp->IsSimulatingPhysics() && CachedVoxelWorld.IsValid())
{
    // 查物品腳下格的材質類別
    FVector BelowPos  = GetActorLocation() - FVector(0.f, 0.f, WorldScale::TileSizeCm * 0.5f);
    FGridPos BelowTile = WorldScale::WorldToTile(BelowPos);
    FTileCell Cell = CachedVoxelWorld->GetTileWorld()->GetCell(BelowTile.X, BelowTile.Y, BelowTile.Z);

    if (FMaterialRegistry::GetPhysics(Cell.MaterialID) == EPhysicsCategory::Liquid)
    {
        // 浮力 = 1.5× 重力，使物品緩慢上浮
        const float GravAcc = 980.f * WorldScale::GlobalGravityScale * WorldScale::GravityScaleMult;
        const float Mass    = MeshComp->GetMass();
        MeshComp->AddForce(FVector(0.f, 0.f, Mass * GravAcc * 1.5f));
    }
}
```

**純 `.cpp` 改動，可 Live Coding。**

---

### PHYS-5：清理舊代碼 + Build + 打包

- 從 `.cpp` 刪除 `PhysicsTick()`、`HandleTileCollision()`、`IsSolidAt()`、`GetBelowRestitution()` 的實作體
- 確認 Build 0 錯誤 0 警告
- 打包 Standalone，驗證驗收清單

---

## 五、實作順序與依賴

```
PHYS-0（Python 建 UPhysicalMaterial 資產）     ← 無 C++ 改動
    ↓
PHYS-1（MaterialRegistry.h 加路徑欄位）        ← Rebuild 1（.h 變動）
    ↓
PHYS-2（GreedyMesher 指定 Physical Material）  ← Live Coding（.cpp only）
    ↓                                            ※ 若 RMC API 不支援，跳過，退回方案 B
PHYS-3（APhysicalItemActor.h/.cpp 大改）       ← Rebuild 2（.h 變動）
    ↓
PHYS-4（Tick 液體浮力 AddForce）               ← Live Coding（.cpp only）
    ↓
PHYS-5（清理 + Build + 打包）
```

**與 GAS 整合的關係**：完全獨立，無交叉依賴。可同期或先後進行，不互相干擾。  
plugin-assessment.html 說的「與 GAS 同期處理」只是效率建議（趁架構乾淨），非技術前置條件。

---

## 六、驗收清單

- [ ] 物品從高處落下，能正確落地（不穿透 RMC tile）
- [ ] 落到石地 vs 沙地，反彈程度明顯不同（PHYS-1/PHYS-2 完成後）
- [ ] 物品落入水中有浮力，不沉底
- [ ] 物品在空中有旋轉 / 滾動動畫（Chaos 自動提供）
- [ ] 攜帶中物品不受重力，跟隨角色移動
- [ ] 投擲後沿正確方向飛出，撞牆後彈跳
- [ ] Build 0 錯誤 0 警告，打包成功

---

## 七、已知限制與後續

| 限制 | 說明 | 後續處理 |
|------|------|---------|
| PHYS-2 依賴 RMC API | 若 RMC 不支援 per-section Physical Material，退回 `OnComponentHit` 方案 B | 實作時確認 RMC 版本 API |
| 液體浮力每幀查 tile | 效能尚可（單物品，非熱路徑），多物品場景若有壓力可改為 Overlap event | 目前 APhysicalItemActor 同時存在數量有限 |
| 旋轉鎖定 | 若投擲物旋轉影響拾取 hit test，可用 `FConstraintInstance` 鎖定旋轉軸 | 視實機測試結果決定 |
