# 建模體素化管線（Mesh Voxelization Pipeline）設計計畫

> 撰寫日期：2026-06-24
> 狀態：設計確認，待實作
> 前置依賴：plan-asset-registry.md（DataAsset 模式）、plan-debris-fragment.md（Phase 3 Voxelization 列）

---

## 一、設計定位

本文件描述以下完整管線：

```
離線工具鏈                      執行期
─────────────────────────────   ──────────────────────────────
FBX / OBJ                       角色 / 武器 / 建築 = 一般 Skeletal/Static Mesh 渲染
  → binvox 自動體素化            ↓ 發生毀壞事件
  → MagicaVoxel 上材質調色盤    FVoxelInjector::Inject()
  → .vox 存入 VoxelAssets/      ↓ 將體素 tile 注入 TileCell grid
  → UVoxelAsset DataAsset       ↓ CA 世界接管（碎裂 / 掉落 / 物理）
                                 ↓ ADebrisActor → FragmentXxx（plan-debris-fragment）
```

### 與其他計畫的關係

| 計畫 | 本計畫如何銜接 |
|------|--------------|
| **plan-asset-registry.md** | `UVoxelAssetRegistry` 遵循相同 DataAsset 模式；`TSoftObjectPtr` + `FStreamableManager` 非同步載入 |
| **plan-debris-fragment.md §七 Phase 3** | 本文件即為「Voxelization」一行的完整實作；注入後的 tile 銷毀事件自動觸發既有 `SpawnDebris()` 管線，debris 系統無需改動 |

---

## 二、術語

| 術語 | 說明 |
|------|------|
| **Display Mesh** | 角色 / 武器 / 建築的正常渲染用 Skeletal / Static Mesh |
| **Voxel Asset** | 同一物件預先體素化後儲存的 tile 資料（`.vox` 格式） |
| **UVoxelAsset** | 包裝 Voxel Asset 的 UE5 DataAsset，含 tile 陣列與尺寸資訊 |
| **Injection** | 毀壞瞬間將 UVoxelAsset 的 tile 資料寫入 `TileCell` grid |
| **Palette Mapping** | `.vox` 調色盤 index（0~255）→ `EMaterialType` 的對應表 |

---

## 三、架構決策

### 可進入建築必須全體素（路線 A），不可用「Mesh + 毀壞時注入」（路線 B）

**情境**：玩家進入城堡室內，在某個房間摧毀一面牆。

路線 B（平時 Mesh，毀壞時注入 tile）在此情境下根本無法運作：

```
注入那面牆後，grid 的視角：
[ Air ][ Air ][ Air ]
[ Air ][牆 tile][ Air ]   ← 地板/天花板/其他牆仍是 Mesh，不在 grid 裡
[ Air ][ Air ][ Air ]

CA 看到：牆 tile 四周全是空氣 → 物理計算結果錯誤
```

正確做法：室內空間從生成起就全是 tile，CA 才有完整空間上下文：

```
[ 石牆 ][ 石牆 ][ 石牆 ]
[ 石牆 ][ 空氣 ][ 石牆 ]   ← 玩家在空氣格裡
[ 地板 ][ 地板 ][ 地板 ]   ← 地板/牆壁/天花板都是 tile
```

玩家打穿牆壁 → `SetTile(Air)` → CA 自動處理碎片掉落、空氣連通、後續物理。

**設計切分原則（按玩家互動程度）：**

| 物件類型 | 方案 | 理由 |
|---------|------|------|
| 遠景裝飾性建築外觀 | Mesh（LOD）| 玩家進不去，只看外觀 |
| 玩家可進入的室內空間 | **全 tile（路線 A）** | 需要完整 CA 物理上下文 |
| 室內精緻裝飾品（雕像、吊燈）| Mesh + 路線 B | 獨立物件，周圍 tile 環境已存在，注入後有上下文 |
| 門、箱子、工作臺等互動道具 | Mesh + 路線 B | 同上 |

> **結論**：建築本體（結構）走路線 A；建築內部的裝飾物件走路線 B。
> 兩者可共存：城堡牆壁是 tile，掛在牆上的燭台是 Mesh。

**Grain 對室內精細度的影響：**

| GrainCurrent | TileSizeCm | 牆壁厚度（3 tile）| 窗框可見性 |
|-------------|-----------|----------------|-----------|
| 16（現在）  | 6.25 cm   | 18.75 cm       | ❌ 不到 1 tile |
| 64（長期）  | 1.5625 cm | 4.7 cm         | ✅ 3~4 tile  |

現階段建築室內結構邏輯正確，視覺細節等 M-10 GPU CA + Grain=64 後自然提升。

---

### 角色 / 武器 / 裝備不體素化為常規渲染

理由：
- `TileSizeCm = 100 / GrainCurrent`（目前 Grain=16 → 6.25 cm/tile）
- 一把劍（80cm 長，4cm 寬）= 13×1×1 tile，細節無法還原
- 人物（200cm 高）= 32 tile 高，曲線輪廓全部鋸齒化

**結論**：平時一律以 Display Mesh 渲染；只有在「需要毀壞並與 CA 世界互動」的瞬間才注入體素。

### 體素化為離線預處理，不在 Runtime 做

原因：Runtime 將 Mesh 轉 tile 的計算量不可預測（大型建築可達數十萬 tile），
會造成主執行緒卡頓。所有體素資料在打包前就準備好，Runtime 只做「注入」動作。

### 地形 / 建築仍可受益於較高精度

武器、人物因尺寸過小難以體素化，但建築 / 地標 / 大型物件在夠高 Grain 下可做到
外觀接近原始建模：

| GrainCurrent | TileSizeCm | 一棟 10m 寬建築的 tile 數 |
|-------------|-----------|------------------------|
| 16（現在）  | 6.25 cm   | 160 tile（尚可）        |
| 64（長期）  | 1.5625 cm | 640 tile（細節豐富）    |

---

## 四、離線工具鏈（Voxelization Pipeline）

### 4-1 工具清單

| 工具 | 用途 | 取得方式 |
|------|------|---------|
| **Blender 4.x**（headless）| FBX → OBJ 轉換（binvox 不吃 FBX）| 免費，官網下載 |
| **binvox** | OBJ → `.binvox`（幾何體素化，二進位佔用格） | 免費，patrickmin.com/binvox |
| **MagicaVoxel** | 開啟 `.binvox`，手動上調色盤（指定材質類型）→ 存成 `.vox` | 免費 |

### 4-2 自動偵測腳本（PowerShell）

```powershell
# scan-unvoxelized.ps1
# 掃描 Content/Meshes/，找出缺少 Content/VoxelAssets/ 對應檔的 FBX

$meshDir  = "C:\SkillCreatorUE5\Content\Meshes"
$voxDir   = "C:\SkillCreatorUE5\Content\VoxelAssets"
$blender  = "C:\Program Files\Blender Foundation\Blender 4.x\blender.exe"
$binvoxExe = "C:\Tools\binvox.exe"
$tmpDir   = "$env:TEMP\vox_tmp"

New-Item -ItemType Directory -Force $tmpDir | Out-Null
New-Item -ItemType Directory -Force $voxDir | Out-Null

foreach ($fbx in Get-ChildItem $meshDir -Recurse -Filter "*.fbx") {
    $base   = $fbx.BaseName
    $binOut = "$voxDir\$base.binvox"
    $voxOut = "$voxDir\$base.vox"

    if ((Test-Path $binOut) -or (Test-Path $voxOut)) {
        Write-Host "[SKIP] $base — 已有體素版本"
        continue
    }

    Write-Host "[TODO] $base — 需要體素化"

    # Step 1: FBX → OBJ（Blender headless）
    $objOut = "$tmpDir\$base.obj"
    & $blender --background --python "$PSScriptRoot\fbx_to_obj.py" `
               -- $fbx.FullName $objOut 2>&1 | Out-Null

    if (-not (Test-Path $objOut)) {
        Write-Warning "  Blender 轉換失敗，跳過"
        continue
    }

    # Step 2: OBJ → .binvox（解析度由物件包圍盒長邊決定）
    # -d N：N 為最長軸的 tile 數，依 TileSizeCm = 6.25 換算
    & $binvoxExe -d 64 $objOut -o binvox 2>&1 | Out-Null
    $generated = "$tmpDir\$base.binvox"
    if (Test-Path $generated) {
        Copy-Item $generated $binOut
        Write-Host "  → $base.binvox 完成（待 MagicaVoxel 上材質）"
    }
}
Write-Host "`n完成。請在 MagicaVoxel 開啟 Content/VoxelAssets/*.binvox，上調色盤後另存為 .vox"
```

### 4-3 Blender headless 轉換腳本（fbx_to_obj.py）

```python
# fbx_to_obj.py — 由 scan-unvoxelized.ps1 呼叫
import bpy, sys

argv  = sys.argv[sys.argv.index("--") + 1:]
src   = argv[0]
dst   = argv[1]

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()
bpy.ops.import_scene.fbx(filepath=src)
bpy.ops.export_scene.obj(filepath=dst, use_materials=False)
```

### 4-4 材質調色盤規範（Palette Convention）

binvox 只輸出幾何佔用格（有/無 voxel），不含材質資訊。
需在 MagicaVoxel 以約定好的調色盤顏色代表 `EMaterialType`：

| Palette Index | 代表色（RGBA） | EMaterialType |
|--------------|--------------|---------------|
| 1 | (128, 85, 50, 255) 木棕 | Wood |
| 2 | (160, 160, 160, 255) 淺灰 | Stone |
| 3 | (80, 120, 60, 255) 土綠 | Dirt |
| 4 | (220, 180, 80, 255) 金黃 | Metal（鋼鐵） |
| 5 | (60, 80, 220, 255) 寶藍 | MagicCrystal |
| 6 | (200, 200, 200, 255) 亮銀 | Ash |
| … | … | … |

> 此調色盤規範對應 `UVoxelMaterialPalette` DataAsset（見第六節）。
> binvox 輸出後，在 MagicaVoxel 用 Bucket Fill 按材質分區上色，最後存成 `.vox`。

---

## 五、.vox 格式說明

MagicaVoxel `.vox`（版本 150）格式結構（用於解析器實作參照）：

```
Magic   : "VOX " (4 bytes)
Version : 150    (int32)

Chunks:
  MAIN
  └─ SIZE { x, y, z : int32 }            // 包圍盒尺寸（tile 數）
  └─ XYZI { numVoxels, [{x,y,z,i}×N] }  // 每個非空 tile 的坐標 + 調色盤 index
  └─ RGBA { 256 × (r,g,b,a) }            // 調色盤（index 1-based）
```

上限：單一 `.vox` 最大 256×256×256 tile。大型建築需要分塊存放（V-4 處理）。

---

## 六、UE5 資料結構

### 6-1 FVoxelCell（最小儲存單元）

```cpp
// Source/SkillCreatorCore/Public/VoxelAsset.h
struct FVoxelCell
{
    int16 X, Y, Z;          // 相對物件原點的 tile 偏移
    EMaterialType Material;  // 材質
};
```

### 6-2 UVoxelAsset（DataAsset）

```cpp
UCLASS(BlueprintType)
class SKILLCREATORCORE_API UVoxelAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    // 所有非空 tile（Local 空間，相對物件 pivot）
    UPROPERTY(EditAnywhere)
    TArray<FVoxelCell> Cells;

    // 包圍盒（tile 單位）— 用於 Injection 時的迭代範圍
    UPROPERTY(EditAnywhere)
    FIntVector BoundsMin;

    UPROPERTY(EditAnywhere)
    FIntVector BoundsMax;

    // 對應的 Display Mesh（建立參照，方便 Editor 對照）
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UStaticMesh> SourceMesh;
};
```

### 6-3 UVoxelMaterialPalette（調色盤映射 DataAsset）

```cpp
// 遵循 plan-asset-registry.md DataAsset 模式
UCLASS(BlueprintType)
class SKILLCREATORCORE_API UVoxelMaterialPalette : public UDataAsset
{
    GENERATED_BODY()
public:
    // Index 0 = 空氣（忽略），1~255 對應 MagicaVoxel 調色盤 index
    UPROPERTY(EditAnywhere)
    TArray<EMaterialType> PaletteToMaterial;  // 長度 256

    EMaterialType Resolve(uint8 PaletteIndex) const;
};
```

### 6-4 UVoxelAssetRegistry（登記表 DataAsset）

```cpp
// 遵循 plan-asset-registry.md §5 其他登記表模式
USTRUCT(BlueprintType)
struct FVoxelAssetEntry : public FTableRowBase
{
    GENERATED_BODY()

    // 對應的 UVoxelAsset
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelAsset> Asset;

    // 採用哪份調色盤（可不同批素材使用不同規範）
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelMaterialPalette> Palette;
};

// RowName = FName asset id（e.g. "sword_iron", "chest_oak", "tower_stone"）
// 使用 UDataTable 讓 Editor 可 CSV 匯入 / 匯出
```

Content 目錄結構：

```
Content/
  VoxelAssets/
    sword_iron.vox
    chest_oak.vox
    tower_stone.vox
  Data/
    DA_VoxelAssetRegistry.uasset   ← UDataTable (FVoxelAssetEntry)
    DA_VoxelMaterialPalette.uasset ← UVoxelMaterialPalette
```

---

## 七、.vox 解析器

```cpp
// Source/SkillCreatorRuntime/Private/VoxParser.cpp
namespace VoxParser
{
    // 從 .vox 二進位資料 + 調色盤映射，輸出 FVoxelCell 陣列
    bool Parse(const TArray<uint8>& Raw,
               const UVoxelMaterialPalette* Palette,
               TArray<FVoxelCell>& OutCells,
               FIntVector& OutBoundsMax);
}
```

實作步驟：
1. 驗證 Magic "VOX " + Version 150
2. 遍歷 Chunks：找 SIZE（取得 BoundsMax）、XYZI（讀 voxel 列表）、RGBA（讀調色盤）
3. 對每個 `{x, y, z, i}`：`Palette->Resolve(i)` → `EMaterialType`，非 Air 才加入 OutCells
4. 注意 MagicaVoxel 座標軸：X=右，Y=深，Z=上（對應到世界座標需轉換）

---

## 八、執行期注入（FVoxelInjector）

```cpp
// Source/SkillCreatorRuntime/Public/FVoxelInjector.h
struct FVoxelInjector
{
    // 將 UVoxelAsset 注入 TileWorld3D
    // WorldTransform：物件的世界座標 Transform（含 Rotation / Scale）
    // DestroyReason：傳給 OnTileDestroyed 的銷毀原因
    static void Inject(
        const UVoxelAsset*   Asset,
        const FTransform&    WorldTransform,
        TileWorld3D*         World,
        EDestroyReason       Reason = EDestroyReason::Slash);
};
```

注入邏輯：

```cpp
void FVoxelInjector::Inject(const UVoxelAsset* Asset,
                             const FTransform& WorldTransform,
                             TileWorld3D* World,
                             EDestroyReason Reason)
{
    if (!Asset || !World) return;

    for (const FVoxelCell& Cell : Asset->Cells)
    {
        // 1. Local tile → Local world cm
        FVector LocalCm = FVector(Cell.X, Cell.Y, Cell.Z) * WorldScale::TileSizeCm;

        // 2. Apply object transform（旋轉 / 縮放 / 位移）
        FVector WorldCm = WorldTransform.TransformPosition(LocalCm);

        // 3. World cm → tile 座標（WorldToTile，參考 WorldScale.h）
        FIntVector TileCoord = WorldScale::WorldToTile(WorldCm);

        // 4. 設定 tile（原本是空氣的位置才填入，避免覆蓋地形）
        if (World->GetTile(TileCoord) == EMaterialType::Air)
        {
            World->SetTile(TileCoord, Cell.Material);
            // OnTileDestroyed 不在這裡觸發（注入是「建立」不是「銷毀」）
        }
    }

    // 5. 注入完成後觸發一次性銷毀事件，讓碎片系統接管
    //    （見下方 ADestructibleMeshActor 的 TriggerVoxelDestruction）
}
```

---

## 九、AVoxelizableActor（基底類別）

所有「需要在 Runtime 評估渲染路徑」的 Actor 繼承此基底類別，持有 `EVoxelizationMode` 與動態切換邏輯。
`ADestructibleMeshActor`（Route B）和未來的 `AVoxelizableLivingActor`（Auto 模式角色）都繼承自它。

```cpp
// Source/SkillCreatorRuntime/Public/AVoxelizableActor.h
UCLASS(Abstract)
class AVoxelizableActor : public AActor
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxelization")
    EVoxelizationMode VoxelizationMode = EVoxelizationMode::Auto;

    // 外部通知：移動狀態改變（角色 locomotion、定身效果等）
    void NotifyMovementChanged(bool bMovingNow);

    // 外部通知：ActorScale 改變（放大 / 縮小效果）
    void NotifyScaleChanged();

protected:
    // 子類實作：切換到全體素展示（Route A）
    virtual void SwitchToFullVoxel()   PURE_VIRTUAL(AVoxelizableActor::SwitchToFullVoxel,);
    // 子類實作：切換回 Mesh 展示（Route B）
    virtual void SwitchToMeshDisplay() PURE_VIRTUAL(AVoxelizableActor::SwitchToMeshDisplay,);

    bool bIsFullVoxel  = false;
    bool bIsMoving     = false;

private:
    void ReevaluatePath();   // 比較精細度 + bIsMoving，決定是否呼叫 Switch*
    float ComputeFidelityTiles() const;
};
```

**`ReevaluatePath()` 邏輯：**

```
精細度 = GetComponentsBoundingBox().GetSize().GetMin() / TileSizeCm

switch (VoxelizationMode):
  ForceAlwaysVoxel:
      if (!bIsFullVoxel) SwitchToFullVoxel()
  Auto:
      bool shouldBeVoxel = !bIsMoving && 精細度 >= WorldScale::MinFidelityTiles
      if (shouldBeVoxel  && !bIsFullVoxel) SwitchToFullVoxel()
      if (!shouldBeVoxel &&  bIsFullVoxel) SwitchToMeshDisplay()
  ForceMeshWithVoxel:
      if (bIsFullVoxel) SwitchToMeshDisplay()
  PureMesh:
      (不呼叫任何切換，子類直接跳過)
```

> `MinFidelityTiles` 預設 8，定義於 `Source/SkillCreatorCore/Public/WorldScale.h`（尚未加入，V-1 時補）。

---

## 十、ADestructibleMeshActor（可毀壞 Mesh Actor）

```cpp
// Source/SkillCreatorRuntime/Public/ADestructibleMeshActor.h
UCLASS()
class ADestructibleMeshActor : public AVoxelizableActor, public IInteractable
{
    GENERATED_BODY()
public:
    // 正常渲染用 Mesh
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* DisplayMesh;

    // 對應的 Voxel Asset ID（查 DA_VoxelAssetRegistry）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxelization")
    FName VoxelAssetId;

    // 觸發毀壞：隱藏 DisplayMesh → 注入體素 → 銷毀 Actor
    // Reason / Intensity：傳給 debris 系統（plan-debris-fragment §四 公式）
    UFUNCTION(BlueprintCallable)
    void TriggerDestruction(EDestroyReason Reason, float Intensity = 1.f,
                            FVector SlashDirection = FVector::ZeroVector);

protected:
    virtual void BeginPlay() override;

private:
    void OnVoxelAssetLoaded();
    TSharedPtr<FStreamableHandle> LoadHandle;
    UVoxelAsset* LoadedAsset = nullptr;
};
```

`TriggerDestruction()` 流程：

```
1. DisplayMesh->SetVisibility(false)
2. LoadedAsset 已快取 → 直接走 3
   未載入       → FStreamableManager::RequestSyncLoad()（毀壞瞬間允許同步）
3. FVoxelInjector::Inject(LoadedAsset, GetActorTransform(), World, Reason)
4. 呼叫 AVoxelWorldActor::TriggerVoxelDestruction(BBox, Reason, Intensity, SlashDir)
   → 走 plan-debris-fragment D-4 SpawnDebris() 管線
5. Destroy()
```

---

## 十一、與 plan-debris-fragment 的接合點

注入後，體素 tile 已在 `TileWorld3D` 裡。
`TriggerVoxelDestruction()` 對注入的包圍盒執行「摧毀所有 tile」：

```cpp
// AVoxelWorldActor 新增方法
void AVoxelWorldActor::TriggerVoxelDestruction(
    FIntVector BoundsMin, FIntVector BoundsMax,
    EDestroyReason Reason, float Intensity, FVector Dir)
{
    TMap<EMaterialType, int32> DestroyedByMat;
    for (int X = BoundsMin.X; X <= BoundsMax.X; ++X)
    for (int Y = BoundsMin.Y; Y <= BoundsMax.Y; ++Y)
    for (int Z = BoundsMin.Z; Z <= BoundsMax.Z; ++Z)
    {
        EMaterialType Mat = TileWorld->GetTile({X, Y, Z});
        if (Mat == EMaterialType::Air) continue;
        TileWorld->SetTile({X, Y, Z}, EMaterialType::Air);
        DestroyedByMat.FindOrAdd(Mat)++;
    }
    // 聚合回呼 → SpawnDebris（plan-debris-fragment D-4）
    for (auto& [Mat, Count] : DestroyedByMat)
    {
        FDebrisParams Params;
        Params.Reason    = Reason;
        Params.Intensity = Intensity;
        Params.Direction = Dir;
        DropManager->SpawnDebris(BoundsMin, Mat, Count, Params);
    }
}
```

**debris 系統本身完全不需要修改**——`ADebrisActor → FragmentXxx` 介面不感知來源是 tile 世界還是注入的 Voxel Asset。

---

## 十二、與 plan-asset-registry 的接合點

| 登記表 | 本計畫新增 | 既有 |
|--------|-----------|------|
| `UTileMaterialRegistry` | 不動 | ✅ |
| `UMobMeshRegistry` | 不動 | ✅ |
| `UVoxelAssetRegistry`（DataTable） | 新增 | — |
| `UVoxelMaterialPalette` | 新增 | — |

非同步預載遵循同一模式：

```cpp
// SkillCreatorGameInstance::PreloadCoreAssets()（plan-asset-registry §六）
// 加入 VoxelAsset 預載（僅常見物件；罕見大型建築在首次 TriggerDestruction 時同步載）
```

---

## 十三、Editor 工作流

毎新增一個可毀壞物件時的流程：

```
1. 準備 FBX（或取得免費模型）
2. 執行 scan-unvoxelized.ps1
   → 生成 .binvox（幾何佔用格）
3. 在 MagicaVoxel 開啟 .binvox
   → 依調色盤規範（§4-4）上色
   → 另存為 Content/VoxelAssets/<name>.vox
4. 在 UE5 Editor 匯入 .vox（寫一個 UFactory 或手動建立 UVoxelAsset DataAsset）
5. 在 DA_VoxelAssetRegistry（DataTable）新增一列
   RowName = asset id，Asset = 上一步的 UVoxelAsset，Palette = DA_VoxelMaterialPalette
6. 在場景中放置 ADestructibleMeshActor
   → DisplayMesh = 正常高品質 Mesh
   → VoxelAssetId = 步驟 5 的 RowName
```

---

## 十四、注意事項

### TileSizeCm 與體素精度

`TileSizeCm = 100 / GrainCurrent`（目前 Grain=16 → 6.25 cm）。

- 小型物件（武器、盔甲）在 Grain=16 精度極差（見下表），體素化僅供毀壞特效，不求還原外觀
- 建築 / 大型環境物件在 Grain=16 已有基本辨識度；Grain=64（長期目標）效果顯著提升

| 物件 | Grain=16（6.25cm）| Grain=64（1.56cm）|
|------|-------------------|-------------------|
| 劍（80cm）| 13×1 tile（棍子）| 51×3 tile（勉強辨識）|
| 盾（50cm）| 8×8×1 tile | 32×32×4 tile |
| 小屋（4m）| 64×64 tile（尚可）| 256×256 tile（細節） |

> **設計決策**：Grain=16 時，武器/裝備的體素化僅用於毀壞效果（碎片散落），不要求外觀還原。

### 大型物件的分塊策略

`.vox` 格式上限 256³ tile。超出此尺寸的建築需分塊：

```
tower_stone_chunk_0_0.vox   // 底部
tower_stone_chunk_0_1.vox   // 中段
tower_stone_chunk_0_2.vox   // 頂部
```

`UVoxelAsset` 支援 `TArray<TSoftObjectPtr<UVoxelAsset>> Chunks` 的巢狀結構（V-4 實作）。

### Rotation 精度

注入時 `WorldTransform.TransformPosition()` 含 Rotation，tile 座標會取整（`FMath::RoundToInt`）。
斜放的物件體素化後會有鋸齒感，這是體素的固有限制，接受即可。

---

## 十五、分階段實作

```
[x] V-0  工具鏈
         ├── scan-unvoxelized.ps1 腳本
         ├── fbx_to_obj.py（Blender headless）
         └── UVoxelMaterialPalette DataAsset + DA_VoxelMaterialPalette.uasset
             ⚠️ DA_VoxelMaterialPalette.uasset 需於 Editor 手動建立（類別已在 SkillCreatorCore 定義）

[x] V-1  資料結構
         ├── FVoxelCell、UVoxelAsset（DataAsset）
         ├── FVoxelAssetEntry（DataTable row）
         └── DA_VoxelAssetRegistry.uasset
             ⚠️ DA_VoxelAssetRegistry.uasset 需於 Editor 手動建立

[x] V-2  解析器 + 靜態測試
         ├── VoxParser::Parse()（.vox 150 格式解析，MagicaVoxel Z-up→Voxel Y-down 座標轉換）
         └── Editor Utility：匯入 .vox → 自動建立 UVoxelAsset（待後續）

[x] V-3  FVoxelInjector
         └── FVoxelInjector::Inject()（local tile→UE5 world cm→Voxel tile，只寫空格，回傳 BoundsMin/Max）

[x] V-4  ADestructibleMeshActor + TriggerDestruction
         ├── AVoxelizableActor 基底（EVoxelizationMode 4 值 + ReevaluatePath + ComputeFidelityTiles）
         ├── ADestructibleMeshActor Route B Actor
         ├── AVoxelWorldActor::TriggerVoxelDestruction（掃 BBox 清 tile + 廣播 OnVoxelDestructionComplete）
         └── UDroppedItemManager::OnWorldBeginPlay 訂閱兩個 delegate（D-3/V-4 delegate 鏈全通）

[ ] V-5  大型物件分塊支援
         └── UVoxelAsset Chunks 巢狀結構（需 3D 美術資產）
```

V-0~V-2 可在無 3D 美術資產的情況下先完成（用 MagicaVoxel 手繪測試 .vox）。
V-3~V-4 需要實際 .vox 測試資產，**且 D-1~D-5 先完成**。
V-5 待建築類素材確認後再做。

---

## 十六、渲染路徑判定機制

### 核心原則

路徑由**體素精細度閾值**動態判定，而非綁死於 class 型別。
地形 / 建築固定走路線 A；其餘物件由系統在 Runtime 評估。

---

### EVoxelizationMode（設計師標記）

每個可體素化的 Actor 持有一個模式標記，設計師在放置時設定：

```cpp
UENUM(BlueprintType)
enum class EVoxelizationMode : uint8
{
    ForceAlwaysVoxel,   // 路線 A 強制：地形、建築結構（室內可破壞空間）
    Auto,               // 系統依精細度 + 移動狀態自動判定（角色、敵人、大型裝置）
    ForceMeshWithVoxel, // 路線 B 強制：小型道具（武器、寶箱、裝飾品）
    PureMesh,           // 永遠 Mesh，不體素化（特效、UI 綁定物件）
};
```

---

### Auto 模式的判定邏輯

```
精細度 = 物件 ActorBounds 最短有意義邊長（cm）/ TileSizeCm

if bIsMoving:
    → Route B（移動中全體素代價太高，每幀需清舊 tile 寫新 tile）
else if 精細度 >= MinFidelityTiles（建議 8，存於 WorldScale）:
    → Route A（全體素展示，現在就是 tile）
else:
    → Route B（Mesh 平時展示，毀壞時注入）
```

**精細度對照（MinFidelityTiles = 8）：**

| 物件 | Grain=16（6.25cm）| Grain=64（1.56cm）| 結果 |
|------|-----------------|-----------------|------|
| 劍（4cm 寬）| 0.6 tile | 2.6 tile | Route B（兩者皆不達標）|
| 普通角色（16cm 寬）| 2.6 tile | 10.2 tile | Grain=64 後 Auto 可升 Route A |
| 巨大化 Boss（200cm 寬）| 32 tile | 128 tile | Route A |
| 一般建築牆（30cm 厚）| 4.8 tile | 19.2 tile | Grain=64 後 Auto 升 Route A |
| 城堡主塔（5m 最短邊）| 80 tile | 320 tile | 兩者皆 Route A |

---

### 移動狀態與精細度的動態切換

物件狀態改變時重新評估：

```
事件                         觸發重評估      可能的路徑切換
──────────────────────────   ────────────   ──────────────
魔法放大（scale 增加）        ✅              Route B → Route A（靜止時）
縮小 / 恢復原尺寸            ✅              Route A → Route B
開始移動（走路 / 追擊）       ✅              Route A → Route B（強制）
死亡 / 凍結 / 定身            ✅              Route B → Route A（靜止，重評精細度）
```

**切換流程（Route B → Route A）：**
```
1. 停止渲染 DisplayMesh
2. 載入 UVoxelAsset（同步，切換瞬間允許）
3. FVoxelInjector::Inject()
4. 標記 bIsFullVoxel = true
```

**切換流程（Route A → Route B）：**
```
1. 清除 TileCell grid 裡此物件佔用的 tile（SetTile → Air）
2. 重新顯示 DisplayMesh
3. 標記 bIsFullVoxel = false
```

> ⚠️ 縮小時的 Route A → B 切換不產生碎塊（這不是「被摧毀」）；
> 被摧毀時才觸發 TriggerVoxelDestruction → SpawnDebris。

---

### 各 class / 情境的預設模式

| 物件類型 | 預設 EVoxelizationMode | 說明 |
|---------|----------------------|------|
| 地形 / 世界生成內容 | `ForceAlwaysVoxel` | MapGenerator3D 直接寫 tile，不是 Actor |
| 可進入建築結構 | `ForceAlwaysVoxel` | 室內 CA 物理需要完整 tile 上下文 |
| 角色 / 敵人 / NPC | `Auto` | 靜止 + 夠大時升 Route A |
| 大型機關 / 裝置 | `Auto` | 同上 |
| 武器 / 裝備 | `ForceMeshWithVoxel` | 精細度不足，強制 Route B |
| 室內裝飾物（雕像、木桶）| `ForceMeshWithVoxel` | 周圍建築已是 tile，注入後有 CA 上下文 |
| 特效 / 技能視覺物件 | `PureMesh` | 不參與 CA 世界 |

---

### MinFidelityTiles 的調整時機

`WorldScale::MinFidelityTiles`（建議預設 8）可隨 GrainCurrent 調整：

- **Grain=16**：MinFidelityTiles=8 → 只有非常大的物件才走 Route A
- **Grain=64**：MinFidelityTiles=8 → 普通人物、中型建築都自動升 Route A
- 調高此值 → 更嚴格（更多物件走 Route B）；調低 → 更多物件走 Route A（精細度較差）

---

### 建築如何走 ForceAlwaysVoxel

建築走路線 A 不靠 Actor，而是**在世界生成時以 tile 形式直接寫進 grid**：

```
MapGenerator3D::ComputeChunkData()
  └─ 生成地形 tile
  └─ 呼叫建築生成器（未來：FBuildingPlacer::Place()）
       └─ 直接 TileWorld3D::SetTile()，不 Spawn AActor
```

設計師若要讓預製建築走路線 A，需先透過工具鏈（§四）轉成 tile 資料，
交由世界生成器注入，而非在場景裡放 StaticMesh Actor。

---

## 十七、本次暫緩

| 項目 | 說明 |
|------|------|
| Chaos Cloth 布料體素化 | plan-debris-fragment §七 另開計畫 |
| MPM 軟體形變 | plan-debris-fragment §七 另開計畫 |
| .vox UFactory（自動匯入）| V-2 後期，目前手動建 DataAsset 即可 |
| Pak 外部 VoxelAsset 包 | 依附 plan-asset-registry AR-D |
| 角色骨骼分段體素化（手臂/腿個別 .vox）| Phase 2 Chaos Fracture 確認後再設計 |
