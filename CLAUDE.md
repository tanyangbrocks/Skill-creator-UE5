# SkillCreator UE5 — Claude Code 工作規則

## 專案概要
Unreal Engine 5.4+ / C++ 技能設計系統。從 Godot 4 遷移而來。
玩家用類 Scratch 積木設計能力，效果與 CA 細胞自動機世界互動，目標支援 Grain 64+ 超大世界。

**GitHub**：https://github.com/tanyangbrocks/Skill-creator-UE5.git
**本地路徑**：`C:\SkillCreatorUE5\`
**Godot 原始碼參照**：`C:\skill-creator\`（封存，邏輯對照用，不再修改）

---

## 必讀文件
- `實作進度.md` — 目前里程碑狀態、最新完成、UE5 待辦（**每次啟動先看這裡**）
- `docs/plan-ue5-migration.md` — 遷移計畫（M-0~M-10），**主要工作指引**
- `docs/plan-ability-system.md` — 能力系統設計規格
- `docs/plan-worldlore-integration.md` — 蒼究世界觀（W 系列）

---

## 強制規則

### 🔴 遷移/修復 Godot 邏輯時，動手前必須先讀 Godot 原始碼
凡是「把 Godot 某功能搬到 UE5」「修復跟 Godot 行為不一致的 bug」「補完某個標記 stub/不完整的項目」，
**寫程式之前一律先用 Read/Grep 實際打開 `C:\skill-creator\` 對應的 `.cs` 檔案**，
找到真正的邏輯（公式、預設值、驗證規則、UI 行為），用檔案+行號佐證後才動手實作。

**不可以**：只憑 UE5 既有的 `docs/plan-*.md` 計畫文件、之前 session 留下的記憶筆記，
或單純常識／直覺去猜 Godot 原本怎麼做。這些次級來源即使看起來描述得很具體，
也可能是過去某次摘要時已經簡化或遺漏的版本——以 Godot 原始碼為唯一真相來源。

實作完成後，回報時要附上對照依據（Godot 檔案:行號 → 行為描述 → UE5 對應實作），
不要只說「已對齊 Godot」卻講不出對照了哪一行。

（曾發生的真實教訓：F1 畫筆工具、採掘/放置形狀半徑、SaveSpell 驗證邏輯，
都因為沒有實際讀 Godot 原始碼，憑記憶筆記猜測寫出跟原版行為不符的簡化版。）

### 🔴 「列出實作計畫」= 建立 `docs/plan-*.md` 檔案
使用者說「列出實作計畫」時，預設動作是建立一份 `docs/plan-<功能名稱>.md` 文件，
不是直接回覆在對話中。命名規則同現有 `docs/plan-*.md` 慣例。

### 🔴 每次實作完成後必須更新 `實作進度.md`
完成任何功能後，立刻更新根目錄的 `實作進度.md`：
- "UE5 目前狀態" 一行摘要
- "UE5 最新完成" 表格新增一列
- "UE5 待辦" 勾選對應項目
- 更新標頭第一行的「最後更新」日期
- 同步在原計畫文件（`docs/plan-ue5-migration.md` 等）的對應項目前加 `[x]` 標記已完成

例外：W 系列世界觀項目（plan-worldlore-integration.md）完成後，
若只是新增 W-N 功能而未改動現有 C++ 系統，不需更新本檔；
plan-worldlore-integration.md 有自己的完成狀態標記。

### 🔴 Commit 粒度
功能完成 + `實作進度.md` 同步 = 一個 commit。
不要把進度更新單獨拆成獨立 commit。

### 🔴 歷史歸檔
「UE5 最新完成」超過 8 筆時，舊紀錄移到 `docs/history/`（保留最新 5 筆）。

### 🔴 Build 必須 0 錯誤 0 警告
每次改完 C++ 都跑 Build，有錯立刻修。

### 🔴 Build / Rebuild 完成後必須順便打包 Standalone
使用者一律用 Standalone 執行檔測試（節省記憶體，不用 PIE）。
每次 Build 或 Rebuild 通過後，立刻用以下命令打包：

```
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="C:\SkillCreatorUE5\SkillCreatorUE5.uproject" -noP4 -platform=Win64 -clientconfig=Development -cook -build -stage -pak -archive -archivedirectory="C:\SkillCreatorUE5\Packaged" -utf8output
```

輸出執行檔：`C:\SkillCreatorUE5\Packaged\Windows\SkillCreatorUE5.exe`  
首次打包約 20~40 分鐘（Cook 所有資產），後續增量 Cook 較快。  
打包過程本身包含 Build 步驟（`-build` flag），所以 .h UPROPERTY 變更也會一起編譯進去，不需要另行 Rebuild。

### 🔴 修改 .h 標頭檔（UCLASS/UPROPERTY/UENUM）必須關 Editor + 完整 Rebuild
不能用 Live Coding 熱編譯 .h 變更（UHT 反射資料不會更新，會無預警 crash）。
只有修改 .cpp 純邏輯才允許 Live Coding。

### 🔴 最小化使用者手動 Editor 操作
使用者在 UE Editor 手動操作（Designer 拖拉、Blueprint 設定、World Settings 調整）
是時間與精力的極致浪費。除非以下情況，否則一律用 C++ 解決：

**唯一真正必須手動的例外：**
- 建立 Material / Texture / Sound 等純美術資產
- 建立 Widget Blueprint / Behavior Tree / Blackboard 等 `.uasset` 本體（但內部設定用 C++ 處理）
- 在 Blueprint Graph 實作 `BlueprintImplementableEvent`（邏輯本體）

**以下一律用 C++ 取代，不要要求使用者手動做：**
- Actor 預設值（`ConstructorHelpers::FClassFinder` / `FObjectFinder` 載入 BP 資產）
- GameMode / HUD / PlayerController Class 指定（constructor 直接設，或 `DefaultGame.ini`）
- Widget 元件位置與 Anchor（`meta=(BindWidget)` + `NativeConstruct()` 設 `UCanvasPanelSlot`）
- Widget 資料綁定（`NativeConstruct()` / `NativeTick()` 直接呼叫 `SetPercent()` / `SetText()`）
- Blueprint 子類只是為了設一個欄位值 → 改成 C++ constructor 或 `ConstructorHelpers`
- World Settings / Project Settings 能用 `.ini` 設定的 → 寫進 `DefaultGame.ini` / `DefaultEngine.ini`
- AIController 的 BehaviorTree 指定（`ConstructorHelpers::FObjectFinder` 載入）
- Component 的任何 `EditAnywhere` 預設值（constructor 直接賦值）

---

## 技術核心決策（詳見 docs/plan-ue5-migration.md §二）

| 系統 | 決策 |
|------|------|
| Tile 渲染 | RealtimeMeshComponent（Mega-Chunk 64³ = 一個 Section） |
| CA 模擬 | CPU（M-3），GPU Compute Shader（M-10） |
| SpellRunner 跨幀 | 手動 Tick 累加器（不用 FLatentActionManager） |
| 積木編輯器 UI | Slate SGraphEditor（runtime FBlockNode ↔ editor UEdGraphNode 分離） |
| FBlockNode 子分支 | TArray\<TUniquePtr\<FBlockNode\>\>（不是值型別陣列） |
| FInstruction Params | 單一 FInstancedStruct Payload（不用 TMap，opcode 告知型別） |
| TileCell | 純 POD（uint8 only），背景 thread FArchive 序列化安全 |
| GPU-CPU 資料邊界 | GameplayBlock = CPU authoritative；VisualCA = GPU sim + async readback |
| interface（純 C++ 系統間） | 純虛擬基類（class IFoo，不用 UINTERFACE） |
| interface（Blueprint/AActor） | UINTERFACE + IFoo 雙生宏 |

## 模組結構

```
SkillCreatorUE5/
├── Source/
│   ├── SkillCreatorCore/       基礎型別、介面（無 gameplay 依賴）
│   ├── SkillCreatorRuntime/    主 Gameplay Module
│   └── SkillCreatorEditor/     積木編輯器（Editor only）
├── Plugins/
│   ├── AbilitySystem/          VM + 技能施放
│   ├── VoxelWorld/             CA 世界 + Chunk + RMC 渲染
│   └── SkillCreatorUI/         積木編輯器 Slate UI
└── docs/                       計畫文件
```

---

## 擴充速查（新增內容時必看）

> **通則**：凡是改 `.h` 裡的 `UENUM / UCLASS / UPROPERTY`，必須關 Editor + 完整 Rebuild（不可 Live Coding）。

### 新增材質 / 地形

1. `Plugins/VoxelWorld/.../MaterialType.h` — 在 `EMaterialType` 加新 enum 值
2. `Plugins/VoxelWorld/.../MaterialRegistry.cpp` — 在 `FMaterialRegistry::Initialize()` 呼叫 `Register()`，填 Physics / Color / Drops
3. （可選）`Plugins/VoxelWorld/.../ElementalReactionTable.cpp` — 若有元素碰撞反應，加到反應表

### 新增物品

1. `Source/SkillCreatorCore/Public/ItemId.h` — 在 `EItemId` 加新值
2. `Source/SkillCreatorCore/Public/ItemRegistry.cpp` — `FItemRegistry::Initialize()` 呼叫 `Register()`，填 `FItemData`（DisplayName / IsPlaceable / IsTool / EquipSlot / AtkMult …）

### 新增敵人類型

1. `Source/SkillCreatorRuntime/Public/AEnemy.h` — 在 `EEnemyType` 加新值
2. `Source/SkillCreatorRuntime/Private/AEnemy.cpp` — 在 constructor 設預設屬性
3. `Plugins/VoxelWorld/.../BTTask_*.cpp` — 新增對應 BT Task（移動 / 攻擊邏輯）
4. `Source/SkillCreatorRuntime/Private/AMobSpawnController.cpp` — 在 MobTable 加 `FMobTableEntry`

### 新增積木（Spell Block）

1. `Source/SkillCreatorCore/Public/BlockNode.h` — 在 `EBlockType` 加新值
2. `Plugins/AbilitySystem/.../SpellCompiler.cpp` — 在 `EmitBlock()` 加 case → 輸出 `FInstruction`
3. `Plugins/AbilitySystem/.../ExecutionLoop.cpp` — 在 `Step()` 加對應 OpCode handler
4. `Plugins/SkillCreatorUI/.../SBlockEditorWidget.cpp` — 在 palette 加積木卡片（顏色 / 名稱 / 預設參數）

### 新增地形特徵（地表水池模式，2026-06-23 修正）

⚠️ 舊版 `FTerrainFeature` 多型基類已移除（`PostProcessRegion()` 從未被呼叫過，是死碼；且 UE5
沒有 Godot 的「初始條帶 PlaceInWorld + 其餘懶加載 GetSurfaceOverride」雙路徑，所有 chunk 一律
經 `ComputeChunkData()` 懶加載生成，虛擬呼叫在這個背景執行緒熱路徑上沒有意義）。
參考 `FSurfaceWaterPool`（`SurfaceWaterPool.h/.cpp`）的模式：

1. 新建 `.h/.cpp`（不繼承任何基類），提供三個方法：`Initialize(Seed,W,H,D)`（從 seed 算佈局，存純資料如 `TArray<FPoolDesc>`）、`Prepare(GetHeight 回呼)`（用地表高度算衍生狀態，例如水面 Y）、`static QueryOverride(const TArray<FPoolDesc>&, wx, wz, NaturalSurfaceY, OutEffectiveY, OutMat)`（純函數，給 `ComputeChunkData()` 每個 tile 查詢，thread-safe）
2. `MapGenerator3D.h` 加成員持有這個特徵物件；`InitTerrainParams()` 呼叫 `Initialize+Prepare`
3. `MapGenerator3D::EnsureChunksAround()` 把佈局資料（`GetXxx()` 的拷貝）存進背景 thread 的 lambda 閉包，傳給 `ComputeChunkData()`
4. `ComputeChunkData()` 在算完 `SurfaceY`（自然地表）後呼叫 `QueryOverride()`，依回傳的 `EffectiveY`/`Mat` 覆寫該 tile 的材質判斷（注意 UE5 Y 增大＝向下，跟 Godot Y-up 相反，公式裡的加減號要反過來）
5. 若世界是無邊界（`WorldWidth/Depth<=0`），佈局生成不能用「離邊界多遠」的邏輯，要改成以 spawn/原點為中心的固定散布半徑

### 新增元素反應

1. `Plugins/AbilitySystem/.../ElementalReactionTable.cpp` — 在 `BuildTable()` 加 `FElementalReaction` 條目（Attacker / Defender / Effect / CaEffect）

### 新增刻印 / 圖騰資料

1. `Plugins/AbilitySystem/Public/TotemLibrary.h/.cpp` — 在 `AllEngravings()` / `AllTotems()` 陣列加條目（2026-06-20 修正：原寫在 `SkillCreatorUI`，但 `FSpellSlotSync`〔AbilitySystem plugin〕需要查這份表，UI plugin 不能反過來被低層 plugin 依賴，故移到 `AbilitySystem`）

### 新增 UI Widget

- 不需要 `.uasset`：在 C++ 繼承 `UUserWidget`，**在 `NativeOnInitialized()`（不是 `NativeConstruct()`！）**呼叫 `BuildLayout()` 程式化建立 WidgetTree，用 `CreateWidget<T>(PC, T::StaticClass())` 實例化
- 參考：`UGameFlowWidget`（已修正用 `NativeOnInitialized()`）。**不要**再參考 `UInputSettingsWidget` / `USpellListWidget`——這兩個目前還是 `NativeConstruct()` 呼叫 `BuildLayout()` 的舊寫法，已知中同一個 bug，待稽核修正（見下方系統性風險提醒）

🔴 **絕對不要在 `NativeConstruct()` 裡呼叫 `BuildLayout()`**：引擎原始碼 `UserWidget.cpp:1203` `RebuildWidget()` 會在 `WidgetTree->RootWidget` 為 null 時回退成空的 `SNew(SSpacer)`，而這個檢查發生在第一次 `AddToViewport()` 觸發的 `TakeWidget_Private()` 裡，順序是「先呼叫 `RebuildWidget()`（這時候抓到 null）→ 結果定型快取 → 才呼叫 `OnWidgetRebuilt()` → `NativeConstruct()`」。也就是說在 `NativeConstruct()` 裡才設定 `RootWidget` 永遠太晚，第一次顯示必定是空畫面，而且因為 `MyWidget` 一旦快取就不會重建，這個空白是永久性的（GameFlowWidget 2026-06-21 踩過，詳見 `docs/開發血汗錄.md` 第 1 案）。`NativeOnInitialized()` 在 `Initialize()`（`CreateWidget()` 當下、早於 `AddToViewport()`）執行，已確認 `CreateWidget<T>(PC, Class)` 一定會先 `SetPlayerContext()` 才 `Initialize()`，保證 `NativeOnInitialized()` 會被呼叫。
⚠️ 專案內舊有用 `NativeConstruct()` 寫的 widget（Inventory/Equipment/Settings/ShapeMenu/SpellGroup/Stats/InputSettings/SpellList/DebugPaint/BlockEditor）可能都中同一個 bug，待逐一稽核改用 `NativeOnInitialized()`。

### 替換 / 新增實體物品 3D Mesh（2026-06-27）

`APhysicalItemActor::Init()` 查找 Mesh 的三層順序：
1. `FItemData::MeshPath`（`ItemRegistry.cpp` 手動設定的精確路徑）
2. `/Game/Items/SM_{EItemId 值名稱}`（慣例路徑，`generate_placeholders.py` 批次建立佔位）
3. Engine BasicShape（Cube / Cylinder / Sphere，依 bIsPlaceable / bIsWeapon / bIsTool / bIsConsumable 自動選取）

**情境 A：替換現有佔位（普通物品）**

使用者提供任意格式資產（FBX / OBJ / MagicaVoxel .vox 匯出的 OBJ）：

1. 確認目標 `EItemId` 的 enum 值名稱（`ItemId.h`），例如 `ToolBasicAxe`
2. 在 `import_assets.py` 加一個 `AssetImportTask`，`destination_path='/Game/Items'`，`destination_name='SM_ToolBasicAxe'`，`replace_existing=True`
3. 在 Editor Python Console 執行 `import_assets.py`（或單獨執行新加的那段）
4. 無需改任何 C++，無需 Rebuild（只是換資產，沒有 .h 變動）

**情境 B：替換主要道具（精確指定路徑）**

道具有自己的專屬資料夾（例如 `/Game/Weapons/DemonSword/SM_DemonSword`）或資產格式特殊：

1. 先把資產匯入到目標路徑（同 import_assets.py 的模式）
2. 在 `ItemRegistry.cpp` 對應 `Set()` 行設定 `D.MeshPath = FSoftObjectPath(TEXT("/Game/.../SM_Xxx.SM_Xxx"))`
3. Build（`ItemRegistry.cpp` 是 .cpp，可 Live Coding）

**情境 C：新增 EItemId 並給它 Mesh**

1. `ItemId.h` 加新值（`.h` 變動 → 必須關 Editor + Rebuild）
2. `ItemRegistry.cpp` 在 `Init()` 用對應 `MakeXxx()` helper 建立 `FItemData`，若有精確路徑設 `D.MeshPath`
3. `generate_placeholders.py` 的 `ALL_ITEMS` 清單補上新名稱
4. Rebuild → 執行 `generate_placeholders.py` 建立佔位（若未設 MeshPath 才需要）

**`generate_placeholders.py` 執行時機**：
- 第一次設定環境，或新增 EItemId 後，在 Editor Python Console 執行一次
- 已存在的資產自動跳過（不會覆蓋手動改過的精緻版本）
- 執行路徑：Output Log → Cmd → `py "C:\SkillCreatorUE5\generate_placeholders.py"`

### 用 Blender 程式化產生 Item Mesh（2026-06-27）

`generate_models.py` 是 Blender 5.1 headless 腳本，為所有 EItemId 產生具識別度的低多邊形 FBX：
- 劍→刀身+護手+握柄；斧→扇形頭+柄；鎬→雙爪+柄；鏟→槳形刀片+長柄
- 頭盔→半球；鎧甲→扁圓柱；褲子→雙腿+腰帶；護符→圓盤+環
- 方塊→倒角正方體；礦石→扁球；碎片→不規則 ico 球；木棍→細圓柱
- 輸出到 `asset/3d/generated/SM_*.fbx`（1 Blender m = 100 UE5 cm，`apply_unit_scale=True`）
- 已存在檔案自動跳過（刪除後重跑才重新生成）
- SKIP 集合排除有真實資產的項目（WeaponWoodSword）

**執行步驟**：

```powershell
# Step 1：Blender 生成模型（約 1~2 分鐘）
& "C:\Program Files\Blender Foundation\Blender 5.1\blender.exe" --background --python "C:\SkillCreatorUE5\generate_models.py"

# Step 2：在 UE5 Editor Python Console 匯入（import_assets.py 的 Step 4 會自動掃 generated/ 目錄）
py "C:\SkillCreatorUE5\import_assets.py"
```

**新增 EItemId 時同步清單**：
在 `generate_models.py` 的 `ITEMS` dict 加一條 `'NewItemName': gen_xxx`，
對應 generator 函式（`gen_block` / `gen_ore` / `gen_sword` 等），刪除 fbx 後重跑即可。

### 世界尺度說明（給未來 AI）

- **Tile 大小**：`TileSizeCm = 100.f / GrainCurrent`（`WorldScale.h:31`），**不是固定常數**。目前 `GrainCurrent = 16` → `TileSizeCm = 6.25 cm`。改 tile 大小只需改 `GrainCurrent`，`TileSizeCm` 及所有衍生值（CapsuleRadius/CapsuleHalfHeight/WalkSpeedCm/JumpZVelocityCm/GravityScaleMult）自動跟進；不需要全局 OriginX/Z，tile 座標 × TileSizeCm 即 UE5 世界座標
- **角色 / 武器 / 裝備**：一律用 Skeletal/Static Mesh 渲染，**不體素化**。只有在觸發毀壞事件時才即時注入體素（見 `docs/plan-voxelization.md`）
- **水平無邊界**：`AVoxelWorldActor::WorldWidth/Depth = 0` = 無限懶載入，`ChunkStreamingManager` 動態 evict；不要在 WorldScale 加固定 WorldW/D 常數
- **垂直上限**：`AVoxelWorldActor::WorldHeight = 256 tiles`（約 16m @ 6.25cm），可直接改此 UPROPERTY
- **GrainCurrent = 16**：目前值。**GrainTarget = 64**：長期目標（M-10+ GPU CA 後）；不要現在用它推導任何數值
