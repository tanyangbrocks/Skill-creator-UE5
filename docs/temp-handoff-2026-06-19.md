# 臨時交接計畫 — 2026-06-19

> 任何 AI 看到此文件，直接從「目前狀態」和「待辦清單」繼續，無需重讀歷史。

---

## 背景摘要

本次任務是接續上個 AI session（被 compact/token 耗盡中斷）的工作，主要完成：
1. WorldScale Grain 可擴縮架構修正
2. 儲存技能整構按鈕（含 SpellSlotSync 5 項驗證）
3. F1/F2/F4 開發者工具
4. 採掘（TickMining）+ 放置（右鍵）
5. issues.md 全項稽核（對照 Godot 原始碼）

---

## 目前狀態（截至 2026-06-19 本 session）

### ✅ 已完成（本 session）

| 步驟 | 說明 | 檔案 |
|-----|------|------|
| WorldScale 修正 | GrainCurrent=16、TileSizeCm=100/Grain（=6.25cm）、PlayerW=GrainCurrent（=16）、GravityScaleMult=TileSizeCm/30、WalkSpeedCm=TileSizeCm*Grain*4=400cm/s、JumpZVelocityCm=TileSizeCm*14 | `WorldScale.h` |
| GravityScale 加入 constructor | `GetCharacterMovement()->GravityScale = WorldScale::GravityScaleMult;` | `ASkillCreatorCharacter.cpp` |
| issues.md F1/F2/F4 訂正 | F1「完全缺失」→「已實作（部分）」；F2「完全缺失」→「已實作」；F4「完全缺失」→「已實作（部分）」 | `docs/issues.md` |
| 交接文件 | 本文件 | `docs/temp-handoff-2026-06-19.md` |

### ✅ 前一個 AI session 已實作（未提交，uncommitted，但程式碼已在工作樹）

| 功能 | 檔案 | 驗證狀態 |
|-----|------|---------|
| FTotemBlockArgs + FEngravingBlockArgs USTRUCT | `Instruction.h` | 已手動讀檔確認 ✅ |
| FSpellSlotSync（SyncSlotsFromBlocks + ValidateSpell + TotemToContainer + HasActivationTypeBlock） | `SpellSlotSync.h`（新增） | 已手動讀檔確認 ✅ |
| SBlockEditorWidget 技能名稱欄 + 儲存按鈕 + 5 項驗證 + ShowValidationErrors | `SBlockEditorWidget.h/.cpp` | 已手動讀檔確認 ✅ |
| OnMine TickMining（漸進採掘）+ 形狀支援 | `ASkillCreatorCharacter.cpp` | 已手動讀檔確認 ✅ |
| OnPlace 右鍵放置（hold-to-place + 節流） | `ASkillCreatorCharacter.cpp` | 已手動讀檔確認 ✅ |
| OnMineReleased / OnPlaceReleased | `ASkillCreatorCharacter.h/.cpp` | 已手動讀檔確認 ✅ |
| F1/F2/F4 開發者工具 handlers | `ASkillCreatorCharacter.h/.cpp` | 已手動讀檔確認 ✅ |
| EuclideanDistance() | `GridPos.h` | 已手動讀檔確認 ✅ |
| SpawnForReason ShapeMining 早退 | `UDroppedItemManager.cpp` | 已手動讀檔確認 ✅ |
| WorldScale MiningRangeTiles + DefaultShapeRadius | `WorldScale.h` | 已手動讀檔確認 ✅ |

### ⏳ 待辦清單（按順序執行）

#### 步驟 A：Build（需要使用者關 Editor）

**重要**：`Instruction.h` 含新 USTRUCT（FTotemBlockArgs + FEngravingBlockArgs），
`WorldScale.h` 是 .h 修改。**必須關 Editor + 完整 Rebuild，不可 Live Coding**。

使用者操作：
1. 關閉 Unreal Editor
2. 執行：`"C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" SkillCreatorUE5Editor Win64 Development -project="C:\SkillCreatorUE5\SkillCreatorUE5.uproject" 2>&1 | Tee-Object -FilePath "C:\SkillCreatorUE5\build_output.txt"`
3. 確認 `build_output.txt` 最後一行 `Result: Succeeded`，0 錯誤 0 警告

#### 步驟 B：更新 `實作進度.md`

在「UE5 目前狀態」行末加：` · WorldScale Grain=16 可擴縮架構 ✅`
在「UE5 最新完成」表格加一列：

```markdown
| WorldScale Grain=16 可擴縮架構 | `WorldScale.h` `ASkillCreatorCharacter.cpp` | GrainCurrent=16（對應 Godot WorldScale.cs Grain=16）；TileSizeCm=100/Grain=6.25cm；PlayerH=32 tile=200cm；PlayerW=16 tile；WalkSpeedCm=400cm/s（4 遊戲單位/s，恆等式）；GravityScaleMult=TileSizeCm/30 使 3 tile 跳躍高度不隨 Grain 改變；宣告順序修正（GrainCurrent 必須在 PlayerW/PlayerH 前）。issues.md F1/F2/F4 狀態訂正。 |
```

#### 步驟 C：Commit

```powershell
git add Source/SkillCreatorCore/Public/WorldScale.h
git add Source/SkillCreatorCore/Public/GridPos.h
git add Source/SkillCreatorRuntime/Public/ASkillCreatorCharacter.h
git add Source/SkillCreatorRuntime/Private/ASkillCreatorCharacter.cpp
git add Source/SkillCreatorRuntime/Private/UDroppedItemManager.cpp
git add Plugins/AbilitySystem/Source/AbilitySystem/Public/Instruction.h
git add Plugins/AbilitySystem/Source/AbilitySystem/Public/SpellSlotSync.h
git add Plugins/SkillCreatorUI/Source/SkillCreatorUI/Public/SBlockEditorWidget.h
git add Plugins/SkillCreatorUI/Source/SkillCreatorUI/Private/SBlockEditorWidget.cpp
git add docs/issues.md
git add docs/temp-handoff-2026-06-19.md
git add 實作進度.md
git add CLAUDE.md
```

Commit message：
```
feat(worldscale+save): Grain=16 可擴縮架構 + 儲存技能整構按鈕 + F1/F2/F4

- WorldScale: GrainCurrent=16、TileSizeCm=100/Grain=6.25cm、PlayerW=Grain
  WalkSpeedCm 改為 TileSizeCm*Grain*4=400cm/s（4 遊戲單位/s，不隨 Grain 改變）
  GravityScaleMult=TileSizeCm/30（配合 JumpZVelocity 實現 3 tile 跳躍高度）
  宣告順序修正：GrainCurrent 移至 PlayerW/PlayerH 之前
- ASkillCreatorCharacter constructor 加 GravityScale = GravityScaleMult
- Instruction.h: FTotemBlockArgs + FEngravingBlockArgs（儲存按鈕設計時參數）
- SpellSlotSync.h (new): SyncSlotsFromBlocks / ValidateSpell（5 項驗證）
- SBlockEditorWidget: 技能名稱欄 + 儲存按鈕 + HandleSaveClicked（SpellSlotSync）
- ASkillCreatorCharacter OnMine: TickMining 漸進採掘 + 形狀支援
- ASkillCreatorCharacter OnPlace: 右鍵放置 + hold-to-place 節流
- F1/F2/F4: OnDebugPaint/Coord/Survival + F1/F2/F4 key 綁定
- GridPos: EuclideanDistance()
- UDroppedItemManager: ShapeMining 早退
- issues.md: F1/F2/F4 狀態訂正（完全缺失 → 已實作/已實作部分）
```

---

## Godot ↔ UE5 對照依據（本次實作）

### WorldScale（Godot WorldScale.cs）
```
Grain = 16                          → GrainCurrent = 16
TileSize = 1f / (float)Grain        → TileSizeCm = 100f / (float)GrainCurrent = 6.25 cm
PlayerH = Grain * 2 = 32 tiles      → PlayerH = GrainCurrent * 2 = 32 tiles = 200 cm
PlayerW = Grain = 16 tiles          → PlayerW = GrainCurrent = 16 tiles = 100 cm
```

### 採掘（Godot PlayerController.cs:442-471 TickMining）
- MiningProgress += speedMult * dt * 60  → UE5 同式
- if MiningProgress >= MatData.Hardness → DestroyTile  → UE5 同式
- ShapeMining per-tile 不生成掉落，只在中心格 SpawnForReason(Mining) 一次 → UE5 同式

### 放置（Godot Main.cs:1250-1281）
- _holdToPlace 決定 rising-edge 或 0.12s 節流  → UE5 HUD->bHoldToPlace
- PlaceCenter = HitCell + FaceNormal  → UE5 同式
- Consume 只扣 1 個，不管形狀放多少格  → UE5 同式

### 儲存技能整構（Godot AbilityEditorUI.cs:1276-1345）
- SaveSpell() 第一步：SyncSlotsFromBlocks() 衍生 Slots/Container
- 驗證 5 項：名稱非空、有 Totem、主動技需發動類型積木、無未綁 MP、能力點未超上限
- 失敗：AcceptDialog 顯示全部錯誤
- 成功：Loadout.SetSlot(_activeEditorSlot, _spell)
→ UE5：FSpellSlotSync（SpellSlotSync.h）完整對應

---

## 已知未完成 / 不在本次範圍的缺口

| 項目 | 原因 |
|-----|------|
| issues.md 全項稽核（Agent 2） | token 耗盡/compact 中斷，未完成 |
| F1 畫筆工具實際繪製邏輯 | 需在 OnMine 加分支，本次只實作 toggle |
| F4 生存 8 項數值完整 | UE5 目前顯示 5 項，2 項（速率）未計算 |
| PlacedObjectRegistry 接入採掘 | 「完美移除」邏輯缺口，已記錄在 ASkillCreatorCharacter.cpp 注解 |
| 積木編輯器 Shipping build 支援 | SGraphEditor 依賴 UnrealEd，需改 runtime UMG 方案 |
