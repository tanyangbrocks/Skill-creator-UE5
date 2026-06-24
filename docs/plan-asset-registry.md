# 素材參照系統（Asset Reference System）實作計畫

> 撰寫日期：2026-06-14（原 Godot 版全面重寫）
> 狀態：設計確認，配合 M-6 渲染模組實作
> 目標：素材路徑不寫死，支援預設包 + 外部素材包（mod 材質包機制）

---

## 一、設計目標（與 Godot 版相同，實作方式改為 UE5 原生）

1. **路徑不寫死** — C++ 只用型別安全的索引（enum / FName），不寫任何字串路徑
2. **可替換** — 只在 Editor 改 DataAsset，C++ 程式碼不動
3. **層疊覆蓋** — 外部素材包（mod）可只替換部分素材，其餘 fallback 到預設包
4. **易維護** — 新增 Tile 種類只改 `EMaterialType`（已在 MaterialType.h）+ DataAsset
5. **非同步載入** — 所有素材走 `FStreamableManager` 非同步載入，不卡主執行緒

---

## 二、Godot → UE5 對照

| Godot 方案 | UE5 方案 | 差異 |
|-----------|---------|------|
| `string` key（`"tile.stone.albedo"`） | `EMaterialType` enum index / `FName` | 型別安全，O(1) 查表 |
| `pack.json` 對照表 | `UDataAsset`（Editor 可視化編輯） | 有型別檢查，不能填錯型別 |
| `GD.Load<T>(path)` | `TSoftObjectPtr<T>` + `FStreamableManager` | 非同步，不強制同步阻塞 |
| `res://` vs `user://` 差異 | 不存在（UE5 統一 `/Game/` 路徑） | 問題消失 |
| AssetRegistry C# 靜態類別 | `UAssetManager`（內建）+ 輕薄自訂 DataAsset | 不需自己實作底層 |
| `user://packs/` 外部包 | UE5 pak 檔掛載（`FPakPlatformFile`） | 更強，支援完整資源壓縮 |

---

## 三、核心架構：DataAsset 登記表

每種素材類型有一個對應的 `UDataAsset` 子類別，在 Editor 中建立資產並填寫對應關係。
GameInstance 啟動時載入這些 DataAsset，之後所有系統透過它查詢素材。

```
Content/
  Data/
    DA_TileMaterialRegistry.uasset   ← UTileMaterialRegistry 的實例
    DA_MobMeshRegistry.uasset        ← UMobMeshRegistry 的實例
    DA_EffectRegistry.uasset         ← UEffectRegistry 的實例
```

---

## 四、UTileMaterialRegistry（M-6 最優先）

Tile 材質的查詢索引直接用 `EMaterialType`（uint8），不需要字串 key。

### 4-1 標頭檔

```cpp
// Plugins/VoxelWorld/Source/VoxelWorld/Public/TileMaterialRegistry.h
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MaterialType.h"          // EMaterialType
#include "TileMaterialRegistry.generated.h"

// 單一 Tile 種類的素材資料
USTRUCT(BlueprintType)
struct FTileMaterialEntry
{
    GENERATED_BODY()

    // Albedo / Surface 主材質（Greedy Mesh 用的 UMaterialInterface）
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UMaterialInterface> SurfaceMaterial;

    // 可選：自發光材質（岩漿、發光礦石等）
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UMaterialInterface> EmissiveMaterial;

    bool IsValid() const { return !SurfaceMaterial.IsNull(); }
};

// Tile 材質登記表 DataAsset
UCLASS(BlueprintType)
class VOXELWORLD_API UTileMaterialRegistry : public UDataAsset
{
    GENERATED_BODY()

public:
    // 陣列 index = EMaterialType 的 uint8 值（直接 O(1) 查表）
    // 長度必須 = (int32)EMaterialType::Count
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FTileMaterialEntry> Entries;

    // 取得材質（EMaterialType::Air 回傳 nullptr，正常）
    UMaterialInterface* GetSurface(EMaterialType Type) const;
    UMaterialInterface* GetEmissive(EMaterialType Type) const;

    // 驗證所有非 Air 的 Entry 都有填寫（Editor Preflight 用）
    bool Validate(TArray<FString>& OutErrors) const;
};
```

### 4-2 實作

```cpp
// TileMaterialRegistry.cpp
UMaterialInterface* UTileMaterialRegistry::GetSurface(EMaterialType Type) const
{
    int32 Idx = (int32)Type;
    if (!Entries.IsValidIndex(Idx)) return nullptr;
    return Entries[Idx].SurfaceMaterial.Get();  // 已同步載入時直接回傳；未載入回 nullptr
}

bool UTileMaterialRegistry::Validate(TArray<FString>& OutErrors) const
{
    bool bOk = true;
    for (int32 i = 1; i < (int32)EMaterialType::Count; ++i) // 跳過 Air(0)
    {
        if (!Entries.IsValidIndex(i) || !Entries[i].IsValid())
        {
            OutErrors.Add(FString::Printf(TEXT("EMaterialType %d 缺少 SurfaceMaterial"), i));
            bOk = false;
        }
    }
    return bOk;
}
```

---

## 五、其他登記表（M-6 之後）

### 5-1 UMobMeshRegistry

生物模型以 `FName` 為 key（mob ID 可由設計者自定，不需要 enum 限制數量）。

```cpp
USTRUCT(BlueprintType)
struct FMobMeshEntry : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) TSoftObjectPtr<USkeletalMesh>       Mesh;
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UMaterialInterface>  Material;
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UAnimBlueprint>      AnimBP;
};

// 使用 UDataTable（Editor 可 CSV 匯入 / 匯出）
// RowName = mob id（e.g. "melee"、"heavy_armored"）
```

### 5-2 UEffectRegistry

技能特效與 CA 特效以 `FName` 為 key。

```cpp
USTRUCT(BlueprintType)
struct FEffectEntry : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;
    UPROPERTY(EditAnywhere) TSoftObjectPtr<USoundBase>     HitSound;
};
```

### 5-3 UVoxelAssetRegistry / UVoxelMaterialPalette

體素化管線的兩張登記表，遵循與 `UTileMaterialRegistry` 相同的 DataAsset 模式。
完整設計見 [`plan-voxelization.md`](plan-voxelization.md) §六、§七。

```cpp
// ── 調色盤（palette index → EMaterialType）──────────────────────────────
USTRUCT(BlueprintType)
struct FVoxelPaletteEntry : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) uint8         PaletteIndex; // 1-255（0 = Air 保留）
    UPROPERTY(EditAnywhere) EMaterialType Material;
};
// DataAsset 名稱：DA_VoxelMaterialPalette.uasset

// ── 體素資產登記表（FName → .vox 解析後的 UVoxelAsset）─────────────────
USTRUCT(BlueprintType)
struct FVoxelAssetEntry : public FTableRowBase
{
    GENERATED_BODY()
    // RowName = VoxelAssetId（e.g. "chest_wood"、"statue_stone"）
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UVoxelAsset> Asset;
    UPROPERTY(EditAnywhere) FVector                     PivotOffset; // 注入時的原點偏移
};
// DataAsset 名稱：DA_VoxelAssetRegistry.uasset
```

`UVoxelAsset` 本身是 `UDataAsset` 子類（持有 `TArray<FVoxelCell>` 和調色盤索引引用），
非同步載入策略與其他軟參照登記表相同（見 §六）。

---

## 六、非同步載入策略

所有 DataAsset 內的素材宣告為 `TSoftObjectPtr`（軟參照），啟動時統一非同步預載。
遊戲執行期不阻塞主執行緒。

```cpp
// SkillCreatorGameInstance.cpp（M-7 存讀檔模組實作時加入）
void USkillCreatorGameInstance::PreloadCoreAssets()
{
    // 取得已在 Editor 設定好的 DataAsset 路徑（GameInstance 的 UPROPERTY）
    TArray<FSoftObjectPath> ToLoad;
    for (const FTileMaterialEntry& Entry : TileMaterialRegistry->Entries)
    {
        if (!Entry.SurfaceMaterial.IsNull())
            ToLoad.Add(Entry.SurfaceMaterial.ToSoftObjectPath());
    }

    // 非同步載入；回調時標記 bTileMaterialsReady = true
    StreamableHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        ToLoad,
        FStreamableDelegate::CreateUObject(this, &USkillCreatorGameInstance::OnTileMaterialsLoaded)
    );
}
```

---

## 七、外部素材包（Mod 材質包）

### Phase A：Editor 內建（M-6 初版）

只有預設包，所有素材在 Content Browser 管理。  
`DA_TileMaterialRegistry` 即是「預設包」。

### Phase B：多 DataAsset 層疊（AR-C 等效）

允許外部素材包提供第二個 DataAsset，在 `UTileMaterialRegistry` 中以 priority 疊加：

```cpp
UPROPERTY(EditAnywhere)
TArray<TSoftObjectPtr<UTileMaterialRegistry>> OverridePacks;  // 由高到低優先序
```

查詢時從高 priority 往下找第一個非空的 entry，找不到 fallback 到預設包。

### Phase C：執行期 pak 掛載（長期，AR-D 等效）

玩家將 `.pak` 放入 `{SaveDir}/Mods/` 後，遊戲啟動時：

```cpp
// Pak 掛載（只列概念，M-10 之後才需要）
FCoreDelegates::OnMountPak.Execute(PakPath, 0, nullptr);
// 掛載後，pak 內的 Content/ 會被 UE5 視為 /Game/ 路徑的一部分
// 外部包的 DataAsset 覆蓋預設包的同名 Entry
```

---

## 八、Editor 工作流（建立 DataAsset 步驟）

> M-6 渲染模組開始前做一次，之後只需填入新 Tile 的素材路徑。

1. Content Browser → 右鍵 → `Miscellaneous` → `Data Asset`
2. 選擇 `UTileMaterialRegistry` 作為 Class
3. 命名為 `DA_TileMaterialRegistry`，存入 `Content/Data/`
4. 打開 DataAsset，Entries 陣列長度設為 `EMaterialType::Count` 的數值
5. 每個 index 填入對應的 `UMaterialInterface`（從 Content Browser 拖入）
6. 在 `BP_SkillCreatorGameInstance`（或 C++ GameInstance）的 Details 面板指定此 DataAsset

---

## 九、Preflight 驗證

M-6 Build 前呼叫 `Validate()`，確認所有 Tile 都有素材：

```cpp
// Editor Utility 或 UE5 Automation Test 呼叫
TArray<FString> Errors;
if (!Registry->Validate(Errors))
{
    for (const FString& E : Errors)
        UE_LOG(LogVoxelWorld, Error, TEXT("[AR Preflight] %s"), *E);
}
```

---

## 十、分階段實作

### AR-A — DataAsset 骨架 ✅

- [x] `TileMaterialRegistry.h/cpp` 建立（`UTileMaterialRegistry : UDataAsset`、`FTileMaterialEntry`、`Validate()`）
- [ ] `DA_TileMaterialRegistry.uasset` 在 Editor 建立（⚠️ **使用者手動**：Content Browser → Data Asset → UTileMaterialRegistry，存到 `/Game/Data/`，Entries 設 17 格）
- [ ] 每個 Entry 填入對應材質（⚠️ **使用者手動**：建立材質資產後逐一拖入）
- Build 0 錯誤 ✅

### AR-B — 接入渲染 ✅

- [x] `GreedyMesher.cpp`：`PolyGroupBuilder.Add(Mat)` 取代固定 `0`（PolyGroup = MaterialID）
- [x] `AVoxelWorldActor.h`：新增 `TObjectPtr<UTileMaterialRegistry> TileMaterialRegistry`
- [x] `AVoxelWorldActor.cpp`：constructor 自動從 `/Game/Data/DA_TileMaterialRegistry` 載入 Registry；BeginPlay 迴圈設定 `EMaterialType::Count` 個 material slot，無 Registry 或 slot 為 null 時 fallback 到 `VoxelMaterial`（現有行為完全保留）
- 行為與 hardcode 路徑版一致，只是來源改為 DataAsset

### AR-C — 多包層疊（M-8 之後）

- `OverridePacks` 陣列支援，查詢邏輯加 priority fallback
- 開發用：建一份 `DA_TileMaterialRegistry_Test` 只覆蓋石頭材質，驗證 fallback 正確

### AR-D — Pak 掛載（M-10 之後）

- 執行期 `FPakPlatformFile` 掛載外部 `.pak`
- 掛載後自動偵測 `{MountPoint}/Data/DA_TileMaterialRegistry_Override.uasset`
- 完成「類 Minecraft 材質包」功能

---

## 十一、本次暫緩

| 項目 | 說明 |
|------|------|
| UMobMeshRegistry | 生物模型路線未定，等 M-4 角色系統後設計 |
| UEffectRegistry | Niagara 特效 M-6 之後才規劃 |
| Pak 掛載（AR-D） | 需 M-10 之後的穩定基礎 |
| UI 圖示 DataTable | M-8 HUD 模組一起處理 |
