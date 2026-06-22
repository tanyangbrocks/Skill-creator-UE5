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

### 新增地形特徵（TerrainFeature）

1. 在 `Plugins/VoxelWorld/Public/` 新建 `.h`，繼承 `FTerrainFeature`，覆寫 `Initialize / Prepare / PlaceInWorld / GetSurfaceOverride`
2. `Plugins/VoxelWorld/.../MapGenerator3D.cpp` — 在地圖生成流程中實例化並呼叫

### 新增元素反應

1. `Plugins/AbilitySystem/.../ElementalReactionTable.cpp` — 在 `BuildTable()` 加 `FElementalReaction` 條目（Attacker / Defender / Effect / CaEffect）

### 新增刻印 / 圖騰資料

1. `Plugins/AbilitySystem/Public/TotemLibrary.h/.cpp` — 在 `AllEngravings()` / `AllTotems()` 陣列加條目（2026-06-20 修正：原寫在 `SkillCreatorUI`，但 `FSpellSlotSync`〔AbilitySystem plugin〕需要查這份表，UI plugin 不能反過來被低層 plugin 依賴，故移到 `AbilitySystem`）

### 新增 UI Widget

- 不需要 `.uasset`：在 C++ 繼承 `UUserWidget`，**在 `NativeOnInitialized()`（不是 `NativeConstruct()`！）**呼叫 `BuildLayout()` 程式化建立 WidgetTree，用 `CreateWidget<T>(PC, T::StaticClass())` 實例化
- 參考：`UGameFlowWidget`（已修正用 `NativeOnInitialized()`）。**不要**再參考 `UInputSettingsWidget` / `USpellListWidget`——這兩個目前還是 `NativeConstruct()` 呼叫 `BuildLayout()` 的舊寫法，已知中同一個 bug，待稽核修正（見下方系統性風險提醒）

🔴 **絕對不要在 `NativeConstruct()` 裡呼叫 `BuildLayout()`**：引擎原始碼 `UserWidget.cpp:1203` `RebuildWidget()` 會在 `WidgetTree->RootWidget` 為 null 時回退成空的 `SNew(SSpacer)`，而這個檢查發生在第一次 `AddToViewport()` 觸發的 `TakeWidget_Private()` 裡，順序是「先呼叫 `RebuildWidget()`（這時候抓到 null）→ 結果定型快取 → 才呼叫 `OnWidgetRebuilt()` → `NativeConstruct()`」。也就是說在 `NativeConstruct()` 裡才設定 `RootWidget` 永遠太晚，第一次顯示必定是空畫面，而且因為 `MyWidget` 一旦快取就不會重建，這個空白是永久性的（GameFlowWidget 2026-06-21 踩過，詳見 `docs/開發血汗錄.md` 第 1 案）。`NativeOnInitialized()` 在 `Initialize()`（`CreateWidget()` 當下、早於 `AddToViewport()`）執行，已確認 `CreateWidget<T>(PC, Class)` 一定會先 `SetPlayerContext()` 才 `Initialize()`，保證 `NativeOnInitialized()` 會被呼叫。
⚠️ 專案內舊有用 `NativeConstruct()` 寫的 widget（Inventory/Equipment/Settings/ShapeMenu/SpellGroup/Stats/InputSettings/SpellList/DebugPaint/BlockEditor）可能都中同一個 bug，待逐一稽核改用 `NativeOnInitialized()`。

### 世界尺度說明（給未來 AI）

- **Tile 大小**：固定 `WorldScale::TileSizeCm = 30f`（Grain=1 基準）。未來縮小 tile 只需改此值並調整碰撞體積；不需要全局 OriginX/Z，tile 座標 × TileSizeCm 即 UE5 世界座標
- **水平無邊界**：`AVoxelWorldActor::WorldWidth/Depth = 0` = 無限懶載入，`ChunkStreamingManager` 動態 evict；不要在 WorldScale 加固定 WorldW/D 常數
- **垂直上限**：`AVoxelWorldActor::WorldHeight = 256 tiles`（約 7.6m @ 30cm），可直接改此 UPROPERTY
- **GrainTarget = 64**：長期目標（M-10+ GPU CA 後）；不要現在用它推導任何數值
