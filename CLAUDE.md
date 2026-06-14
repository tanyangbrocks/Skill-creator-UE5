# SkillCreator UE5 — Claude Code 工作規則

## 專案概要
Unreal Engine 5.4+ / C++ 技能設計系統。從 Godot 4 遷移而來。
玩家用類 Scratch 積木設計能力，效果與 CA 細胞自動機世界互動，目標支援 Grain 64+ 超大世界。

**Godot 原始碼參照**：`C:\Users\譚揚勳\skill-creator\`（封存，邏輯對照用，不再修改）

---

## 必讀文件
- `docs/plan-ue5-migration.md` — 遷移計畫（M-0~M-10），**主要工作指引**
- `docs/plan-ability-system.md` — 能力系統設計規格
- `docs/plan-worldlore-integration.md` — 蒼究世界觀（W 系列）

---

## 強制規則

### 🔴 Build 必須 0 錯誤 0 警告
每次改完 C++ 都跑 Build，有錯立刻修。

### 🔴 修改 .h 標頭檔（UCLASS/UPROPERTY/UENUM）必須關 Editor + 完整 Rebuild
不能用 Live Coding 熱編譯 .h 變更（UHT 反射資料不會更新，會無預警 crash）。
只有修改 .cpp 純邏輯才允許 Live Coding。

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
