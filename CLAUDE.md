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
