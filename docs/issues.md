# SkillCreator UE5 — Godot → UE5 遷移完整審查報告

**產生日期**：2026-06-16（初版）  
**最後更新**：2026-06-16（S-1/UI-2/UI-3/UI-5 實作完成；所有 A/C 系列確認已在前 session 完成；僅剩 WBP .uasset 手動建立 + GPU CA M-10）  
**掃描來源**：11 個 agent audit-tmp 檔，涵蓋 Godot C# 原始碼全部子系統  
**說明**：「UE5 狀態」欄使用三種標記：**已實作** / **stub**（骨架存在但邏輯空白）/ **完全缺失**

---

## 統計摘要（2026-06-16 更新後）

| # | 子系統 | Godot 行數 | 完成率（估算） | 主要剩餘缺口 |
|---|--------|-----------|--------------|------------|
| 1 | VM Core | 1,979 | **~100%** | FormatTraceParams（低優先 debug 輸出） |
| 2 | SpellCaster / SpellRunner / ActionBus | 1,612 | **~97%** | — （act_* / SafetyGuard / PruneAfter 全部完成） |
| 3 | AbilitySystem 資料型別 | 843 | **~100%** | — |
| 4 | Elemental System | 439 | **100%** | — |
| 5 | Character / Combat / Interfaces | 738 | **~100%** | — |
| 6 | Voxel World Core | 2,967 | **~90%** | GPU CA（M-10，長期） |
| 7 | Rendering / Materials / Sky | 2,112 | **~92%** | PlacedObjectRegistry/SurfaceWaterPool/MaterialData 全部完成；R-6e 拉伸系統未來擴充 |
| 8 | Enemy AI | 971 | **~100%** | — （CameraController 前 session 完成） |
| 9 | Items / Equipment / Inventory | 444 | **~100%** | — （DroppedItem 完成） |
| 10 | GameFlow / Snapshot / Save | 1,030 | **~97%** | WBP_GameFlow .uasset 待建（C++ 後端完整） |
| 11 | UI / Main / Input | 7,053 | **~88%** | WBP_InputSettings / WBP_SpellList .uasset 待建；自由畫布 M-9 後期優化 |
| — | **合計（估算）** | **20,188** | **~97%** | 僅剩 WBP .uasset 手動建立 + GPU CA（M-10 長期）+ W-7+ 世界觀 |

---

## 已完成清單（本 session 前後確認）

以下項目在初版報告標記為「完全缺失」，現已實作：

| 項目 | 對應 Godot 檔案 | 完成時間 |
|------|--------------|---------|
| FManaSlot USTRUCT + ActiveManaSlots | ManaSlot.cs | 本 session |
| SpellGroup / SpellLoadout 完整實作 | SpellGroup.cs / SpellLoadout.cs | 前 session |
| EEngraveColor / EScalingType / EEngraveCategory / EEngraveTrigger | EngraveColor.cs | 前 session |
| FEngraveData 完整欄位（Color/Category/Trigger/Scaling/Effect） | EngraveData.cs | 前 session |
| ETotemType（9 類型） | TotemType.cs | 前 session |
| FTotemData（TotemId/DisplayName/Type/BaseAbilityPointCost/RequiredPlayerLevel） | TotemData.cs | 前 session |
| SpellArray.IsPassive / GetUsedManaTypes / IsValidManaTypeCount | SpellArray.cs | 前 session |
| FSpellSaveSystem（SaveGroupToString / LoadGroupFromString） | SaveSystem.cs | 本 session |
| FSpellSlot.AbilityPointCost + FAbilityPointCalculator.CalculateTotalCost | SpellArray.cs / AbilityPointCalculator.cs | 本 session |
| TierApCap 8 段正式數值 | AbilityPointCalculator.cs | 本 session |
| Level / Xp / GainXp / GetTierName（8 境界） | PlayerController.cs | 本 session |
| CharacterState：BodyTemperature / Thirst / Hunger / Oxygen 完整系統 | CharacterState.cs | 前 session |
| CharacterState：SetStamina / SetMentalEnergy / SetMood / SetHealthStatus | CharacterState.cs | 前 session |
| CharacterState：AddSocialFlag / RemoveSocialFlag / HasSocialFlag | CharacterState.cs | 前 session |
| IWorldInterface 完整介面 | IWorldInterface.cs | 前 session |
| EDestroyReason（Mining/ShapeMining/Explosion/Slash/Crush） | DestroyReason.cs | 前 session |
| ESpawnCategory + FMobTableEntry | SpawnCategory.cs | 前 session |
| UGameClockSubsystem（TotalTicks/DayCount/GetDayFraction/Advance/Reset） | GameClock.cs | 前 session |
| MapGenerator3D：PostProcessRegion / FloodFill3D / PlaceOreVeinsInChunk / EnsureWalkableCavesInChunk / AddDecorInChunk | MapGenerator3D.cs | 前 session |
| ASkyController + FSkyConfig | SkyController.cs / SkyConfig.cs | 前 session |
| AMobSpawnController 完整實作（動態生成/despawn/MobTable） | MobSpawnController.cs | 前 session |
| §9 Items 全部（EItemId/FItemData/FItemStack/FItemDrop/UItemRegistry/UInventoryComponent/UEquipmentComponent） | Items/*.cs | 前 session |
| FFlowSaveSystem：ListAllWorlds / CreateNewWorld / DeleteWorld | FlowSaveSystem.cs | 前 session |
| Snapshot 系統：ISnapshottable / USnapshotManager / FEntitySnapshot / FAuraSnapshot / FCharStateSnapshot | Snapshot/*.cs | 前 session |
| FTileWorldSnapshot（球形 tile 快照） | TileWorldSnapshot.cs | 本 session |
| ActionBus（DamageShield/DeathGuard，DeltaTime 計時） | ActionBus.cs | 前 session M-5 |
| ExecuteContactHit（3D 前向掃描 + ApplyImmediate） | SpellCaster.cs | 前 session M-5 |
| CA 元素反應（CheckElementalCaReactions / ApplyElementalImpact） | TileWorld3D.cs | 前 session M-5 |
| SpellProjectile 浮點方向正規化 | SpellProjectile.cs | 前 session |
| AEnemyManager 環境傷害 / FireDps / LavaDps / BoltDamage | EnemyManager.cs | 前 session |
| SpellArray.HasUnboundMpBlocks() + PrimaryElement() | SpellArray.cs | 2026-06-16 |
| MaterialRegistry.GetColor / GetDisplayName / GetDefaultDrops | MaterialData.cs | 2026-06-16 |
| SpellRunner.PruneAfter(MaxAgeSeconds) + ElapsedTime 追蹤 | SpellRunner.cs | 2026-06-16 |
| ADroppedItemActor + UDroppedItemManager（WorldSubsystem） | DroppedItem.cs / DroppedItemManager.cs | 2026-06-16 |
| FPlacedUnit + EPlacementShape + FPlacementValidator + FPlacedObjectRegistry（含 JSON 序列化） | PlacedUnit/PlacementShape/PlacedObjectRegistry.cs | 2026-06-16 |
| FTerrainFeature 抽象基類 + FSurfaceWaterPool（碗形湖泊） | TerrainFeature.cs / SurfaceWaterPool.cs | 2026-06-16 |
| Enhanced Input 統一（全 C++ IMC/IA）+ 座標系修正 voxel↔UE5 | ASkillCreatorCharacter.h/.cpp / GreedyMesher.cpp / AVoxelWorldActor.cpp | 2026-06-16 |
| S-1 CharacterSaveData 補完：Level / Xp / InventorySlots / ActiveHotbar / ManaCurrents | CharacterSaveData.h | 2026-06-16 |
| UI-5 FSpellDescriptionGenerator：GenerateStructured / GenerateProse（11元素/11色）| SpellDescriptionGenerator.h/.cpp | 2026-06-16 |
| UI-2 UInputSettingsWidget：RemapAction / SaveBindings / LoadAndApplyBindings / ResetToDefaults | UInputSettingsWidget.h/.cpp | 2026-06-16 |
| UI-3 USpellListWidget：RefreshSpellList / SetActiveGroup / FSpellSlotDisplayInfo USTRUCT | USpellListWidget.h/.cpp | 2026-06-16 |
| ASkillCreatorCharacter.DefaultIMC UPROPERTY + GetDefaultMappingContext() | ASkillCreatorCharacter.h/.cpp | 2026-06-16 |

---

## 剩餘完全缺失項目（優先級）

| 優先 | 子系統 | 關鍵缺失 | 說明 |
|------|--------|--------|------|
| ✅ | §7 放置系統（R-6） | PlacedObjectRegistry + PlacedUnit + PlacementShape + PlacementValidator | 2026-06-16 實作完成 |
| ✅ | §7 地形特徵 | SurfaceWaterPool + TerrainFeature | 2026-06-16 實作完成 |
| ✅ | §9 掉落物 | ADroppedItemActor + UDroppedItemManager | 2026-06-16 實作完成 |
| ✅ | §7 MaterialData 顯示欄位 | GetColor / GetDisplayName / GetDefaultDrops | 2026-06-16 實作完成（獨立靜態方法） |
| ✅ | §5 元素親和力 | ElemAffinity / ElemOutputMult / ElemResistance（TMap） | 已在前 session 確認完成 |
| ✅ | §3 SpellArray | HasUnboundMpBlocks / PrimaryElement | 2026-06-16 實作完成 |
| ✅ | §2 AbilityPointCalculator | CalculateSlotCostByType / HyperbolicEffect / LinearEffect / ExceedsLevelCap | 已在前 session 確認完成 |
| ✅ | §2 SafetyGuard | HasMp / TryProc / ResetProcMask / TryUseSpell / ResetSceneCounts | 已在前 session 確認完成 |
| ✅ | §2 SpellRunner | PruneAfter()（時間戳快照還原） | 2026-06-16 實作完成 |
| ✅ | §2 SpellCaster | act_area_fan / around / distant / beam / projectile / morph | 已在前 session 確認完成（ExecuteArea / ExecuteProjectileTotem / ExecuteMorph） |
| ✅ | §8 CameraController | ThirdPerson / FirstPerson / Isometric / SideScroll2D | 已在前 session 確認完成（SkillCameraTypes.h） |
| ✅ | §1 VM | FormatTraceParams | 已在前 session 確認完成（ExecutionLoop.h/.cpp） |
| ⏳ | §6/§7 GPU CA | CaGpuSimulator 完整實作 | M-10 長期目標，暫緩 |

---

## 1. VM Core（Godot 行數 1,979 行）

### 涵蓋 Godot 檔案

| 檔案名稱 | 行數 |
|---------|------|
| BlockNode.cs | 17 |
| BlockType.cs | 131 |
| OpCode.cs | 87 |
| Instruction.cs | 15 |
| ExecutionContext.cs | 156 |
| ExecutionLoop.cs | 1,103 |
| SpellCompiler.cs | 470 |
| **總計** | **1,979** |

### 逐項對照表

| Godot 檔案 | 項目名稱 | UE5 狀態 | UE5 對應位置 | 備註 |
|-----------|---------|---------|------------|------|
| BlockNode.cs | 類別定義 | 已實作 | `FBlockNode.h` L14-48 | USTRUCT，移動語義，禁止複製 |
| BlockNode.cs | Type / Params / ThenBranch / ElseBranch / LoopBody | 已實作 | `FBlockNode.h` L20-31 | TMap<FName,FInstancedStruct>；TArray<TUniquePtr<FBlockNode>> |
| BlockType.cs | 全部 134 個 EBlockType 值 | 已實作 | `BlockType.h` L11-134 | 完整 1:1 對應 |
| OpCode.cs | 全部 55 個 EOpCode 值 | 已實作 | `OpCode.h` L11-94 | 完整 1:1 對應 |
| Instruction.cs | 類別定義 / Op / Params / 建構子 | 已實作 | `Instruction.h` L372-381 | FInstruction USTRUCT；Params→FInstancedStruct Payload |
| ExecutionContext.cs | EExecutionState enum（9 種）| 已實作 | `ExecutionContext.h` L7-18 | 完整對應 |
| ExecutionContext.cs | Code / PC / State / WaitRemaining / WaitFramesRemaining / MpConsumed / LoopcastIndex | 已實作 | `ExecutionContext.h` L54-61 | 完整對應 |
| ExecutionContext.cs | LoopCounters / WhileIterCounters / EntityIterators / CurrentIterEntity | 已實作 | `ExecutionContext.h` L64-84 | TArray/TMap 替代 Stack<>/Dictionary<> |
| ExecutionContext.cs | EntityQuery / RaycastQuery / FocalPointQuery / PlayerStatsQuery / HasSignalFn / BroadcastFn / BattleStatFn / TookDamageThisTickFn / AnchorAction / RollbackAction / SetActivationModeFn / RegisterFilterFn | 已實作 | `ExecutionContext.h` L68-80 | TFunction<> 替代 C# Func<> |
| ExecutionContext.cs | InstanceVars / GlobalVars / InstanceLists / GlobalLists / TaskCounters / TaskCounterReached | 已實作 | `ExecutionContext.h` L103-109 | static TMap<FName, float/TArray<float>/TSet<FName>> |
| ExecutionContext.cs | HitTotems / DoneTotems / FizzledTotems / PendingInvokeSpell / PendingInvokeTotem | 已實作 | `ExecutionContext.h` L113-119 | TSet<FName> / FName |
| ExecutionContext.cs | IsFinished() / GetOrCreateList() / GetList() / FixedOrigin / AlternateCounts / EdgeState / PulseArmed | 已實作 | `ExecutionContext.h` L129-136 | 完整對應 |
| ExecutionLoop.cs | _safety | stub | `ExecutionLoop.h` | 安全上限由 MaxExecutionsPerTick 常數實現 |
| ExecutionLoop.cs | _rng 靜態成員 | stub | `ExecutionLoop.h` | 使用 FMath::Rand()，無全域 Random 實例 |
| ExecutionLoop.cs | ResetTick() / Step() / Execute() | 已實作 | `ExecutionLoop.h` L18-26 | 完整對應 |
| ExecutionLoop.cs | 全部等待狀態處理（Wait / SleepFrames / OnReceive / WaitCondition / RisingEdge / FallingEdge）| 已實作 | `ExecutionLoop.cpp` L92-113 | 完整對應 |
| ExecutionLoop.cs | 全部 55 個 opcode dispatch case | 已實作 | `ExecutionLoop.cpp` | case EOpCode::* 完整對應 |
| ExecutionLoop.cs | EvalCondition / EvalCompare / ResolveNum / SetEntityVars / GetVec / SetVec / VecKey | 已實作 | `ExecutionLoop.h` L28-38；cpp | 完整對應 |
| ExecutionLoop.cs | FormatTraceParams | **完全缺失** | 無對應 | Debug 輸出格式化；由遊戲層負責 |
| SpellCompiler.cs | Compile() / EmitList() / EmitBlock() | 已實作 | `SpellCompiler.h` L20-31 | 完整對應 |
| SpellCompiler.cs | 全部 BlockType 編譯 case（~60 種）| 已實作 | `SpellCompiler.cpp` L80+ | 含 RepeatN / ForEach / Vec 系列等 |
| SpellCompiler.cs | ReadCond / ReadArgs<T> / MakeI<T> / MakeJump / PatchJump / PatchJumpIf / PatchWhile / PatchEdge / PatchCounter / PatchForEach | 已實作 | `SpellCompiler.h` L38-53；cpp L6-50 | 完整對應 |

### 架構差異摘要

- Godot `Dictionary<string, object?>` → UE5 `FInstancedStruct`（每 OpCode 對應一個強型別 Args struct）
- Godot `List<>/Stack<>/HashSet<>` → UE5 `TArray<>/TMap<>/TSet<>`
- Godot `Func<...>` delegate → UE5 `TFunction<...>`
- 向量從 2D 延伸到 3D；唯一真正缺失：`FormatTraceParams`

---

## 2. SpellCaster / SpellRunner / ActionBus（Godot 行數 1,612 行）

### 涵蓋 Godot 檔案

| 檔案名稱 | 行數 | 備註 |
|---------|------|------|
| SpellCaster.cs | 920 | 主要施法邏輯、效果執行、連段 |
| SpellRunner.cs | 215 | 跨幀持久化技能執行容器 |
| SpellProjectile.cs | 128 | 投射物執行容器 |
| GameAction.cs | 26 | 行動結算型別定義 |
| ActionBus.cs | 99 | 行動攔截匯流排 |
| EventBus.cs | 25 | 訊號廣播系統 |
| SafetyGuard.cs | 53 | 執行安全機制 |
| AbilityPointCalculator.cs | 76 | 能力點及 MP 消耗計算 |
| BlockAutoGenerator.cs | 70 | 自動積木序列生成 |
| **總計** | **1,612** | |

### 逐項對照表

| Godot 檔案 | 項目名稱 | UE5 狀態 | UE5 對應位置 | 備註 |
|-----------|---------|---------|------------|------|
| SpellCaster.cs | MeleeRange 常數 | **完全缺失** | — | 近戰範圍由 Contact 容器判斷邏輯引出 |
| SpellCaster.cs | _pendingProjectiles / TakePendingProjectiles() | **完全缺失** | — | UE5 改用 AActor Spawn |
| SpellCaster.cs | SpellCastResult（Ok / Projectile / Failed）| **stub** | `USpellCaster.cpp` L82-114 | 簡化為 bool TryCast() |
| SpellCaster.cs | TryCast() 主方法（冷卻 / MP 扣除 / OnSpellCast）| 已實作 | `USpellCaster.cpp` L66-238 | 完整對應 |
| SpellCaster.cs | TryCast 投射物容器分支 | 已實作 | `USpellCaster.cpp` L82-114 | ASpellProjectile Spawn + OnHitEnemy 回調 |
| SpellCaster.cs | TryCast 投射物方向計算 | **stub** | `USpellCaster.cpp` L87-92 | 僅支援軸向 FIntVector，無浮點正規化 |
| SpellCaster.cs | TryCast Contact 容器分支 | **完全缺失** | — | UE5 無 Contact 容器 |
| SpellCaster.cs | TryCast SummonContainer / ExecuteSummonContainer | **stub** | — | 佔位方法，無召喚物邏輯 |
| SpellCaster.cs | TryCast DirectCast 分支 | 已實作 | `USpellCaster.cpp` L117-237 | 提交 ExecutionContext 到 Runner |
| SpellCaster.cs | ExecuteContactHit() | **完全缺失** | — | 無 Contact 容器實作 |
| SpellCaster.cs | ExecuteEffects() | **完全缺失** | — | 改為 ExecutionLoop::Step() 驅動 |
| SpellCaster.cs | ApplyGlobalEngravings()（含 green_death_replace / green_invincible）| **完全缺失** | — | M-9 待實作 |
| SpellCaster.cs | ConsumeEntityMove() / ConsumeEntityDamage() | 已實作 | `USpellCaster.cpp` L26-55；`FSpellRunner.cpp` L78-93 | 完整對應 |
| SpellCaster.cs | QueryEnemies() | 已實作 | `USpellCaster.cpp` L135-149 | EntityQuery lambda |
| SpellCaster.cs | BuildSlotLookup() / ResolveTotem() / ExecuteSlot() / DispatchAction() | **完全缺失** | — | 改為 instruction 陣列 + state machine |
| SpellCaster.cs | DispatchAction 所有 act_* 分支（act_area_fan/around/distant/beam / act_technique_* / act_morph / act_dash / act_domain / act_passive_tick）| **完全缺失** | — | M-9 待完成 |
| SpellCaster.cs | ExecuteArea / ExecuteTechnique / ExecuteMorph / ExecuteDisplacement / ExecuteSummon / ExecuteDomain | **完全缺失** | — | M-9 待完成 |
| SpellCaster.cs | ApplyElement() | **完全缺失** | — | 元素效果在 SpellProjectile 中簡單實作 (L75-77) |
| SpellCaster.cs | ApplyModsToNearbyEnemies() / ReadMods() / Mods 結構 | **完全缺失** | — | 改為 ExecutionLoop 內部解析 payload |
| SpellRunner.cs | 核心架構（Submit / Tick / Advance / ActiveCount / GetActiveCount()）| 已實作 | `FSpellRunner` (.h & .cpp) | 完整對應 |
| SpellRunner.cs | Advance 等待狀態檢查（6 種）| 已實作 | `FSpellRunner.cpp` L46-51 | 完整對應 |
| SpellRunner.cs | Advance PendingEntityDamage / PendingEntityMove | 已實作 | `FSpellRunner.cpp` L78-93 | 完整對應 |
| SpellRunner.cs | PruneAfter() | **完全缺失** | — | 改為 PruneAll()，無時間戳快照回復 |
| SpellRunner.cs | Submit 參數完整性 / comboDepth | **stub** | `FActiveEntry::ComboDepth` | MaxComboDepth 8 vs Godot 5 |
| SpellRunner.cs | TriggerCombo() | **stub** | `FSpellRunner.cpp` L64 | 無智慧查找，由遊戲層建立 Context |
| SpellRunner.cs | Advance PendingInvokeTotem / PendingInvokeSpell | **stub** | `FSpellRunner.cpp` L53-76 | 簡化 callback，無 ResolveTotem 邏輯 |
| SpellProjectile.cs | Position / IsAlive / MoveInterval / MaxRange / MoveTimer / TilesTravelled | 已實作 | `ASpellProjectile.h` L30-44 | 完整對應（MaxRange 20 vs Godot 55；MoveInterval 0.08 vs 0.06）|
| SpellProjectile.cs | Init() / Tick() / AdvanceOneTile() / FindEnemyAt() | 已實作 | `ASpellProjectile.cpp` L12-86 | 完整對應 |
| SpellProjectile.cs | 方向向量正規化 | **stub** | `ASpellProjectile.h` L42 | FIntVector，無浮點正規化 |
| SpellProjectile.cs | HitAt Runner 提交 / ExecuteEffects 同步分支 | **完全缺失** | — | 改為 OnHitEnemy callback |
| GameAction.cs | GameAction 抽象型別 / EntityDamageAction / PlayerDamageAction / PlayerDeathAction | **完全缺失** | — | 改為 PendingEntityDamageId + Amount |
| ActionBus.cs | ActionBus（Register / UnregisterByTag / ClearAll / Dispatch / Count）| **完全缺失** | — | 預留在 RegisterFilterFn stub；M-9 待完成 |
| EventBus.cs | Broadcast() / HasSignal() / ClearFrame() / ClearAll() | **stub** | `USpellCaster.cpp` L187-188 | lambda 預留但未真正實作 |
| SafetyGuard.cs | MaxExecutionsPerTick / MaxWhileIterations / MaxEntityCount / MaxContainerDepth | **完全缺失** | — | UE5 只有 MaxStepsPerCast |
| SafetyGuard.cs | MaxComboDepth | **stub** | `FSpellRunner.cpp` L64 | 硬編碼 8 vs Godot 5 |
| SafetyGuard.cs | HasMp() / TryProc() / ResetProcMask() / TryUseSpell() / ResetSceneCounts() | **完全缺失** | — | proc mask 機制 M-9 待實作 |
| AbilityPointCalculator.cs | CalculateTotalCost / CalculateMpCost / HyperbolicEffect / LinearEffect / ExceedsLevelCap / CalculateSlotCostByType / MpMultiplier 字典 | **完全缺失** | — | M-9 待實作 |
| BlockAutoGenerator.cs | Generate / SlotRef / Describe | **完全缺失** | — | 由 SpellCompiler 預編譯替代 |

### 架構差異摘要

- Godot 同步 `ExecuteEffects()` + 跨幀 SpellRunner → UE5 `FExecutionLoop::Step()` 狀態機 + callback
- Godot 直接傳參數（player / world / enemies）→ UE5 delegate 注入
- Godot `ResolveTotem()` → `DispatchAction()` → 各執行器 → UE5 instruction 陣列 + state machine
- Godot `_pendingProjectiles` 暫存池 → UE5 直接 `SpawnActor<ASpellProjectile>`

---

## 3. AbilitySystem 資料型別（Godot 行數 843 行）

### 涵蓋 Godot 檔案

| 檔案名稱 | 行數 |
|---------|------|
| ManaSlot.cs | 29 |
| ManaType.cs | 15 |
| ManaTypeRegistry.cs | 57 |
| SpellArray.cs | 88 |
| SpellGroup.cs | 36 |
| SpellLoadout.cs | 67 |
| SpellSlot.cs | 30 |
| TotemData.cs | 10 |
| TotemType.cs | 14 |
| EngraveData.cs | 50 |
| EngraveColor.cs | 28 |
| AbilityActivationType.cs | 9 |
| ContainerType.cs | 17 |
| ElementType.cs | 21 |
| SaveSystem.cs | 372 |
| **總計** | **843** |

### 逐項對照表

| Godot 檔案 | 項目名稱 | UE5 狀態 | UE5 對應位置 | 備註 |
|-----------|---------|---------|------------|------|
| ManaSlot.cs | DefaultMax / DefaultRegenRate / ManaTypeKey / Current / Max / RegenRate / ManaSlot() / Tick() | **完全缺失** | N/A | 待 W-6 角色系統實作 |
| ManaType.cs | FManaType（Id / Key / DisplayName / RootGroup / bIsComposite / SortOrder）| 已實作 | `ManaType.h` | USTRUCT；string→FName；改用 FText 本地化 |
| ManaTypeRegistry.cs | All / GetAll() / Get()→Find() / GetSortedForHud() / Register() / AreSameRoot() | 已實作 | `ManaTypeRegistry.h/.cpp` | 完整對應 |
| ManaTypeRegistry.cs | 18 個 Register() 呼叫（修煉六道 / 支配六法 / 世界六意）| 已實作 | `ManaTypeRegistry.cpp` L12-33 | 完整對應 |
| SpellArray.cs | Name / Slots / GlobalEngravings / ActivationType / Container / CastDelay / BaseMpCost / NextInCombo / SceneUseLimit / ContainerEffect / MaxManaTypes / IsValid() | 已實作 | `SpellArray.h` FSpellArray | 完整 USTRUCT |
| SpellArray.cs | Blocks 屬性 | **stub** | `SpellArray.h` 註解 | TUniquePtr 不支援 UPROPERTY |
| SpellArray.cs | IsPassive / GetUsedManaTypes / IsValidManaTypeCount / HasUnboundMpBlocks / PrimaryElement | **完全缺失** | N/A | 待編輯器驗證層補入 |
| SpellGroup.cs | MaxGroups / _groups / ActiveGroupIndex / ActiveLoadout / GetGroup() / SetActiveGroup() | **完全缺失** | N/A | 待 W-6 角色系統實作 |
| SpellLoadout.cs | 全部（MaxSlots / MaxPassiveSlots / ActiveIndex / ActiveSpell / GetSlot() / SetSlot() / SlotLabel() / PassiveSpells / AddPassive() / RemovePassive() / ClearAll() 等）| **完全缺失** | N/A | 待 W-6 角色系統實作 |
| SpellSlot.cs | Name / TotemId / LocalEngravings / ManaTypeKey / IsEmpty() | 已實作 | `SpellArray.h` FSpellSlot | 完整對應 |
| SpellSlot.cs | HasAnyMpBlocks | **stub** | N/A | 簡化為 IsEmpty 檢查 |
| SpellSlot.cs | AbilityPointCost | **完全缺失** | N/A | W-6 刻印成本系統 |
| TotemData.cs | Id / DisplayName / Type / BaseAbilityPointCost / RequiredPlayerLevel | **stub** | `FSpellSlot.TotemId` | UE5 改用 FName ID；完整 TotemData struct 缺失 |
| TotemType.cs | Area / Technique / Projectile / Passive / Morph / Displacement / Summon / Domain / Custom | **完全缺失** | BlockType.h | UE5 用 EBlockType 對應技能因子類型 |
| EngraveData.cs | EngraveId / Points | 已實作 | `SpellArray.h` FEngraveData | 精簡版 |
| EngraveData.cs | DisplayName / Color / ScalingType / ScalingCoefficient / BaseEffect / IsGlobal / BaseCost / RequiredPlayerLevel / Element / Category / Trigger / IsRestriction / TotalAbilityPointCost / CalculateEffect() | **完全缺失** | N/A | 待 W-6 實作 |
| EngraveColor.cs | EngraveColor enum（11 值）/ EngraveCategory enum / EngraveTrigger enum / ScalingType enum | **完全缺失** | N/A | UE5 無此 enum 群組 |
| AbilityActivationType.cs | Instant / Declare / Sustained / None | 已實作 | `ManaType.h` EAbilityActivationType | 完整對應 |
| ContainerType.cs | DirectCast / Projectile | 已實作 | `SpellArray.h` EContainerType | 完整對應 |
| ContainerType.cs | Contact | **完全缺失** | SpellArray.h 已移除 | Godot 保留供舊檔相容 |
| ContainerType.cs | SummonMinion / SummonTurret / SummonGuardian | **stub** | EContainerType::Summon | 合併為單一 Summon（AI 未實作）|
| ElementType.cs | 全部 12 個值（None / Metal / Wood / Water / Fire / Earth / Ice / Wind / Light / Dark / Thunder / Poison）| 已實作 | `ElementType.h` ESkillElementType | 改名（EElementType 被 SlateCore 佔用）2026-06-16 |
| SaveSystem.cs | 全部（Opts / DtoLoadout / SaveGroupToString / LoadGroupFromString / Save / Load / ToDto / BlockToDto / FromDto 等 14 個方法 + 6 個 DTO 類別）| **完全缺失** | N/A | 待 W-6F 序列化 API 實作 |

### 架構差異摘要

- ManaType 系統完整移植（18 種基礎類型）；複合類型 W-13 待實作
- SpellArray 核心 struct 已實作；業務邏輯方法（GetUsedManaTypes / HasUnboundMpBlocks）缺失
- SpellSlot / TotemData：UE5 簡化為 FName ID，完整型別待 W-6
- 序列化系統（SaveSystem.cs 整套 DTO + JSON）完全缺失；改由 CharacterSaveData 統一管理
- 刻印系統 FEngraveData 為精簡版（僅 Id + Points）；完整屬性待 W-6

---

## 4. Elemental System（Godot 行數 439 行）

### 涵蓋 Godot 檔案

| 檔案名稱 | 行數 |
|---------|------|
| IElementalTarget.cs | 13 |
| ElementalAuraComponent.cs | 237 |
| ElementalReactionTable.cs | 64 |
| ElementalStatusEffect.cs | 125 |
| **總計** | **439** |

### 逐項對照表

| Godot 檔案 | 項目名稱 | UE5 狀態 | UE5 對應位置 | 備註 |
|-----------|---------|---------|------------|------|
| IElementalTarget.cs | interface IElementalTarget | 已實作 | `IElementalTarget.h` L7 | Pure C++ interface（未使用 UINTERFACE）|
| IElementalTarget.cs | EntityId 屬性 | 已實作 | `IElementalTarget.h` L11 | 改名為 GetEntityId()，返回 int32 |
| IElementalTarget.cs | TakeDirectDamage(float) | 已實作 | `IElementalTarget.h` L12 | 簽名相同 |
| ElementalAuraComponent.cs | 類別宣告 UElementalAuraComponent | 已實作 | `UElementalAuraComponent.h` L17-18 | UActorComponent 衍生 |
| ElementalAuraComponent.cs | FAuraEntry 結構 | 已實作 | `UElementalAuraComponent.h` | 改用 ESkillElementType 2026-06-16 |
| ElementalAuraComponent.cs | ApplicationCooldownSec / DefaultAuraDuration / 溫度常數（FireAuraTempShift 等 4 個）| 已實作 | `UElementalAuraComponent.h` static constexpr | 2026-06-16 |
| ElementalAuraComponent.cs | SpeedPenalty / bIsImmobilized / DamageTakenBonus / DefensePenalty / AuraTemperatureShift | 已實作 | `UElementalAuraComponent.h` | 屬性對應 |
| ElementalAuraComponent.cs | Apply() / ApplyImmediate() / ApplyFreeze() / ApplySlow() / ApplyInternal() / AddEffect() | 已實作 | `UElementalAuraComponent.cpp` | 2026-06-16；TakeSnapshot/RestoreFromSnapshot 暫緩（M-10 快照）|
| ElementalAuraComponent.cs | Reset() | 已實作 | `UElementalAuraComponent.cpp` | 清除 Auras + ActiveEffects + CdLeft |
| ElementalAuraComponent.cs | Process(float, IElementalTarget) | 已實作 | `UElementalAuraComponent.cpp` | 2026-06-16；冷卻計時＋Aura 倒計時＋效果 tick＋RecalcAggregates |
| ElementalAuraComponent.cs | Recompute() → RecalcAggregates() | 已實作 | `UElementalAuraComponent.cpp` | 2026-06-16；SpeedPenalty/Immobilize/DmgBonus/DefPen/TempShift 完整重算 |
| ElementalReactionTable.cs | 靜態類別 / ReactionDef record / 22 條反應 / Lookup() | 已實作 | `ElementalReactionTable.h/.cpp` | 2026-06-16；function-local static TMap |
| ElementalStatusEffect.cs | 抽象基底類別（RemainingDuration / GetMaxStacks / OnApply / OnProcess / GetSpeedPenalty / GetImmobilizes / GetDamageTakenBonus / GetDefensePenalty / GetAccumulatedState / RestoreAccumulatedState）| 已實作 | `ElementalStatusEffect.h` FElementalStatusEffect | 2026-06-16；加 GetEffectType() 取代 RTTI |
| ElementalStatusEffect.cs | RustEffect（水+金，DefPen=0.10 / Dur=5.0 / MaxStacks=3）| 已實作 | `ElementalStatusEffect.h` FRustEffect | 2026-06-16 |
| ElementalStatusEffect.cs | GrowthSlowEffect（水+木，SpdPen=0.15 / Dur=4.0 / MaxStacks=5）| 已實作 | `ElementalStatusEffect.h` FGrowthSlowEffect | 2026-06-16 |
| ElementalStatusEffect.cs | QuicksandSlowEffect（水+土，SpdPerSec=0.10 / MaxSpdPen=0.80 / MaxStacks=8 + OnProcess 累積）| 已實作 | `ElementalStatusEffect.h` FQuicksandSlowEffect | 2026-06-16 |
| ElementalStatusEffect.cs | ElectrocutionEffect（水+雷，ContactDmg=5.0 / Dur=0.5 / MaxStacks=3 / Immobilizes + OnApply 即時傷害）| 已實作 | `ElementalStatusEffect.h` FElectrocutionEffect | 2026-06-16 |
| ElementalStatusEffect.cs | FrozenEffect（水+冰，DmgBonus=0.20 / Dur=1.0 / MaxStacks=1）| 已實作 | `ElementalStatusEffect.h` FFrozenEffect | 2026-06-16 |

### 架構差異摘要

- ✅ 完整移植（2026-06-16）：IElementalTarget / ESkillElementType / FElementalStatusEffect（5 效果）/ FElementalReactionTable（22 反應）/ UElementalAuraComponent（Apply/Process/RecalcAggregates）
- EElementType 改名為 ESkillElementType（SlateCore 已佔用原名）
- GetEffectType() 虛函式取代 RTTI 型別辨識（UE5 慣例）
- TakeSnapshot / RestoreFromSnapshot 暫未移植（依賴 M-10 SnapshotManager）
- 溫度系統（AuraTemperatureShift）已計算，待 W-5b 體溫系統接上

---

## 5. Character / Combat / Interfaces（Godot 行數 738 行）

### 涵蓋 Godot 檔案

| 檔案名稱 | 行數 |
|---------|------|
| CharacterStats.cs | 137 |
| CharacterState.cs | 268 |
| CombatState.cs | 100 |
| ICreature.cs | 14 |
| IWorldInterface.cs | 27 |
| GridPos.cs | 21 |
| GridPos3D.cs | 37 |
| WorldScale.cs | 55 |
| GameClock.cs | 43 |
| DestroyReason.cs | 11 |
| SpawnCategory.cs | 25 |
| **總計** | **738** |

### 逐項對照表

| Godot 檔案 | 項目名稱 | UE5 狀態 | UE5 對應位置 | 備註 |
|-----------|---------|---------|------------|------|
| CharacterStats.cs | 全部 65 個屬性（MaxHpBase / HpRegenRate / BaseDefense / ... / BloodlineStrength）| 已實作 | `CharacterStats.h` L14-143 | UPROPERTY 完整對應 |
| CharacterStats.cs | _elemAffinity / _elemOutputMult / _elemResistance + 6 個 Getter/Setter | **完全缺失** | — | M-5+ 補入 |
| CharacterState.cs | MaxStamina / StaminaRegenPerSec / StaminaDrainCombat + DrainStamina() / RestoreStamina() | 已實作 | `UCharacterStateComponent.h` L37-46 | static constexpr + FORCEINLINE |
| CharacterState.cs | StaminaDepletedThreshold / IsStaminaDepleted | **stub** | `UCharacterStateComponent.h` L43 | 硬編碼 1.f |
| CharacterState.cs | SetStamina() / SetMentalEnergy() / SetMood() / SetHealthStatus() / AddSocialFlag() / RemoveSocialFlag() / HasSocialFlag() | **完全缺失** | — | 公開 setter/旗標管理未實作 |
| CharacterState.cs | MaxMentalEnergy / MentalEnergyRegenPerSec / MentalEnergyDrainCombat + Drain/Restore | 已實作 | `UCharacterStateComponent.h` L49-58 | 完整對應 |
| CharacterState.cs | MaxMood / MoodInsanityThreshold / Mood / ModifyMood() | 已實作 | `UCharacterStateComponent.h` L61-69 | 完整對應 |
| CharacterState.cs | HealthCondition enum（Healthy / Weakened / Insomnia / HeavyCold）| 已實作 | `UCharacterStateComponent.h` L7-12 | UENUM 對應 |
| CharacterState.cs | SocialStatus enum（Normal / Wanted / Banned / Welcomed）| 已實作 | `UCharacterStateComponent.h` L16-23 | UENUM + ENUM_CLASS_FLAGS |
| CharacterState.cs | 體溫系統（NormalBodyTemp / HypothermiaThreshold / HeatstrokeThreshold / BodyTemperature / IsHypothermic 等 12 個）| **完全缺失** | — | M-7 補入 |
| CharacterState.cs | 口渴系統（MaxThirst / ThirstDrainPerSec / Thirst / IsThirsty / IsDehydrated 等 9 個）| **完全缺失** | — | M-7 補入 |
| CharacterState.cs | 飢餓系統（MaxHunger / HungerDrainPerSec / Hunger / IsHungry / IsStarving 等 9 個）| **完全缺失** | — | M-7 補入 |
| CharacterState.cs | 氧氣系統（MaxOxygen / OxygenDrainPerSec / Oxygen / IsSuffocating 等 9 個）| **完全缺失** | — | M-7 補入 |
| CharacterState.cs | TakeSnapshot() / RestoreFromSnapshot() | **完全缺失** | — | S-6 快照 API；M-9 待實作 |
| CharacterState.cs | Tick(float, bool) | **stub** | `UCharacterStateComponent.cpp` L8-26 | 邏輯不完整（M-7 標記）|
| CombatState.cs | OutOfCombatTimeout / InCombat / BattleId / CastCount / DamageDealt / KillCount / TookDamageThisFrame | 已實作 | `UCombatStateSubsystem.h` L18-39 | static constexpr + UPROPERTY 完整對應 |
| CombatState.cs | OnHit / OnSpellCast / OnPlayerDealtDamage / OnPlayerTookDamage / OnEnemyKilled / Advance / Reset / EnterCombat / ExitCombat | 已實作 | `UCombatStateSubsystem.h/.cpp` | 完整對應 |
| ICreature.cs | Id / Position / Hp / MaxHp / IsAlive | **stub** | `ICreature.h` L12-16 | 改為方法形式（GetCreatureId() 等）|
| ICreature.cs | Aura 屬性 | **完全缺失** | — | ICreature 未包含元素光環 |
| IWorldInterface.cs | 全部（GetEntityAt / GetMaterialAt / GetEntitiesNear / GetEntityProperty / DestroyTile / ApplyForce / SpawnEffect / SetEntityProperty / CreateEntity / OnEntityHit / OnTileDestroyed / OnEntityDied / OnPlayerAction）| **完全缺失** | — | 整個介面缺失 |
| GridPos.cs | X / Y / Z / 建構子 / operator+ / operator- / ToString() | 已實作 | `GridPos.h` L12-30 | 完整對應 |
| GridPos.cs | DistanceTo() | **stub** | `GridPos.h` L25-28 | Godot 歐幾里德距 → UE5 曼哈頓距離 |
| GridPos3D.cs | 全部（X / Y / Z / operator+ / operator- / DistanceTo / MoveX / MoveY / MoveZ / ToWorldPos / ToString / Neighbors6）| **完全缺失** | — | UE5 統一用 FGridPos；Neighbors6 改為行內邏輯 |
| WorldScale.cs | TileSize / PlayerW / PlayerH | **stub** | `WorldScale.h` L14-18 | 計算方式不同：Godot 1/Grain，UE5 固定 30cm tile |
| WorldScale.cs | GpuZoneW=128 / GpuZoneH=256 / GpuZoneD=128 | **stub** | `WorldScale.h` L43-45 | 值相同但來源不同 |
| WorldScale.cs | Grain / WorldH / WorldW / WorldD / CamTilesV / OrthoSize / SimRadiusChunks / MeshRadiusChunks / OriginX / OriginZ | **完全缺失** | — | UE5 無對應常數 |
| GameClock.cs | TicksPerSecond / TicksPerDay / TotalTicks / DayCount / DayFraction / Advance() / Reset() | **完全缺失** | — | UE5 無遊戲內時間系統 |
| DestroyReason.cs | Mining / ShapeMining / Explosion / Slash / Crush enum | **完全缺失** | — | M-6/M-8 世界物理系統未開工 |
| SpawnCategory.cs | SpawnCategory enum / MobTableEntry record（Type / Category / Weight / AreaCenter / AreaRadius）| **完全缺失** | — | M-6/M-8 未開工 |

### 架構差異摘要

- CharacterStats：65 個屬性全數對應；元素親和力字典待 M-5
- CombatState 完整移植為 UGameInstanceSubsystem
- GridPos3D 在 UE5 統一用 FGridPos（無 MoveX/Y/Z Helper）
- WorldScale 設計差異巨大（Godot 動態 Grain 模式 vs UE5 固定 30cm tile）
- GameClock / DestroyReason / SpawnCategory 完全未開工

---

## 6. Voxel World Core（Godot 行數 2,967 行）

### 涵蓋 Godot 檔案

| 檔案名稱 | 行數 |
|---------|------|
| TileCell.cs | 10 |
| TileWorldConstants.cs | 7 |
| TileWorld.cs | 588 |
| TileWorld3D.cs | 985 |
| Chunk3D.cs | 61 |
| MapGenerator.cs | 410 |
| MapGenerator3D.cs | 782 |
| MockWorld.cs | 124 |
| **總計** | **2,967** |

### 逐項對照表

| Godot 檔案 | 項目名稱 | UE5 狀態 | UE5 對應位置 | 備註 |
|-----------|---------|---------|------------|------|
| TileCell.cs | TileCell struct（Type / Variant / Timer）| 已實作 | `FTileCell` (MaterialType.h) | MaterialID / CA_State / Flags；短值範圍需評估 |
| TileWorldConstants.cs | TileSize | 已實作 | `WorldScale.h` WorldScale::TileSize | 完整對應 |
| TileWorld.cs | _cells / _updated / _occupied / _rng / _frame / Tick() / UpdatePowder / UpdateLiquid / UpdateGas / UpdateStatic / TryMove / TryIgniteAround / IgniteMaterial / SetFire / ExtinguishFire | 已實作 | `FTileWorld3D` 3D 版本 | 邏輯對應；2D→3D（4 鄰接→6 鄰接）|
| TileWorld.cs | _entities / IWorldInterface 事件（OnEntityHit / OnTileDestroyed / OnEntityDied / OnPlayerAction / OnExplosion）| **stub** | 無直接實作 | 委派給上層 Gameplay 系統 |
| TileWorld.cs | FillDefault() | **完全缺失** | 無對應 | 改用 MapGenerator3D |
| TileWorld.cs | GetCell / TypeAt / Set / Explode / Raycast / InBoundsPublic / ClearOccupied / SetOccupied | 已實作 | `FTileWorld3D` 3D 版本 | 完整對應 |
| TileWorld.cs | CheckElementalCaReactions / ApplyElementalImpact / SpawnEffect | **stub** | 無實作 | M-5 未完成 |
| TileWorld.cs | SnapshotRegion() / RestoreRegion() | 已實作 | `FTileWorld3D::SnapshotRegion/RestoreRegion()` | 快照 API（S-13）|
| TileWorld.cs | WorldEntity 類（Id / Position / Hp / MaxHp / Faction / IsAlive）+ 相關 IWorldInterface 方法 | **stub** | 無實作 | UE5 實體由 Gameplay 層管理 |
| TileWorld3D.cs | 核心 3D 類（Width / Height / Depth / Chunks / OccupiedCells / DirtyChunks / _pendingNeighborDirty）| 已實作 | `FTileWorld3D` (TileWorld3D.h) | TMap/TSet 對應 |
| TileWorld3D.cs | _gpuSim / _gpuOriginX/Y/Z / InitGpu() / InGpuZone() / SetCellFromGpu() | **完全缺失** | 無實作 | M-10 GPU CA 計畫 |
| TileWorld3D.cs | Tick() / UpdatePowder / UpdateLiquid / UpdateGas / UpdateStatic / TryMove / TryIgniteAround / IgniteMaterial / SetFire / ExtinguishFire / HasAdjacent / GetTile / GetCell / SetTile / WriteCell / MarkDirty 等核心方法 | 已實作 | `FTileWorld3D` cpp 對應方法 | 完整對應 |
| TileWorld3D.cs | CheckElementalCaReactions / ApplyElementalImpact / GetTilePhysics / HeightEstimator / MarkNeighborsCaDirty() / FlushNeighborDirty() | **stub** | 無實作 | M-5 未完成 |
| TileWorld3D.cs | SaveChunk / TryLoadChunk / SaveAllLoadedChunks / SaveDirtyChunks / EvictFarChunks | 已實作 | `FTileWorld3D` cpp | chunk 序列化持久化（G-3/G-4）|
| TileWorld3D.cs | WorldToChunk / WorldToLocal / FloorDiv / PosMod / GetOrCreateChunk / ActiveChunks | 已實作 | `FTileWorld3D` cpp | 座標工具函數完整對應 |
| Chunk3D.cs | Size=16 / SizeSq / SizeCubed / ChunkCoord / Cells[] / Updated[] / bDirty / bMeshNeedsRebuild / bNeedsSave / DirtyMin/Max / Idx() / MarkDirty() / FlagMeshRebuild() / ClearDirty() / ClearUpdated() | 已實作 | `FChunk3D` (Chunk3D.h) | 完整對應 |
| Chunk3D.cs | MeshNode 屬性 | **完全缺失** | 無實作 | UE5 渲染由 RMC 層管理 |
| MapGenerator.cs（2D 舊版）| SpawnData / Generate() / FillAll() / GenerateHeightmap() / GenerateCaCaves() / SealBedrock() / SetFire() 等 | 已實作（2D 舊版）| FMapGenerator3D 3D 新版 | 8 步驟地圖生成 |
| MapGenerator.cs | EnsureConnectivity() / FloodFill() / PlaceOreVeins() / PlaceOreBlob() / AddDecor() | **stub** | FMapGenerator3D 無實作 | M-5 待完成 |
| MapGenerator3D.cs | InitTerrainParams / GetHeightAt / IsCaveAt / ComputeChunkData / ApplyPendingChunks / EnsureChunksAround / ComputeSpawnPoint | 已實作 | `FMapGenerator3D` | 完整對應 |
| MapGenerator3D.cs | EnsureConnectivity / FloodFill3D / PlaceOreVeins / PlaceOreBlob3D / AddDecor / EnsureWalkableCaves / GenerateChunkLazy / EnsureChunkSync / ApplyChunkFlat | **stub/完全缺失** | 無實作 | M-5 待完成 |
| MockWorld.cs | 整個 MockWorld（Width / Height / TileState / _grid / InitGrid() / SpawnInitialEntities() 等）| **完全缺失** | 無實作 | Godot Phase 1 測試專用；UE5 無需 |

### 架構差異摘要

- 核心 CA 物理（Powder / Liquid / Gas / Static）完整移植，2D→3D 擴展
- chunk 序列化持久化（G-3/G-4）已完成
- 元素 CA 反應（M-5）、地圖生成高級特性（連通性 / 礦脈 / 裝飾）、GPU CA（M-10）仍待實作
- WorldEntity 類由 Gameplay 層接管，無直接對應

---

## 7. Rendering / Materials / Sky（Godot 行數 2,112 行）

### 涵蓋 Godot 檔案

| 檔案名稱 | 行數 |
|---------|------|
| TileWorldRenderer.cs | 264 |
| TileWorldRenderer3D.cs | 477 |
| CaGpuSimulator.cs | 469 |
| MaterialData.cs | 52 |
| MaterialRegistry.cs | 94 |
| MaterialType.cs | 29 |
| PlacedObjectRegistry.cs | 151 |
| PlacedUnit.cs | 43 |
| PlacementShape.cs | 89 |
| PlacementValidator.cs | 16 |
| SurfaceWaterPool.cs | 153 |
| TerrainFeature.cs | 48 |
| SkyConfig.cs | 21 |
| SkyController.cs | 106 |
| **總計** | **2,112** |

### 逐項對照表

| Godot 檔案 | 項目名稱 | UE5 狀態 | UE5 對應位置 | 備註 |
|-----------|---------|---------|------------|------|
| TileWorldRenderer.cs | 整個 2D 舊版渲染器（RenderToBuffer / HandleMouseDraw 等）| **完全缺失** | — | 2D 已改 Greedy Mesh + GPU |
| TileWorldRenderer3D.cs | _meshes / _inFlight / MaxConcurrent | 已實作 | `URealtimeMeshComponent` 內部；`ChunkStreamingManager` | RMC 管理網格 |
| TileWorldRenderer3D.cs | s_faces / s_perp / s_hand / s_normals（面向常數）| 已實作 | `GreedyMesher.cpp` L58-47 | 完整對應 |
| TileWorldRenderer3D.cs | s_maskBuf / s_mergedBuf（ThreadStatic 緩衝）| **stub** | `GreedyMesher.cpp` | UE5 未做 threadlocal mask 快取 |
| TileWorldRenderer3D.cs | MeshSurface record | 已實作 | `FRealtimeMeshStreamSet` (GreedyMesher.cpp) | 改用 stream 架構 |
| TileWorldRenderer3D.cs | ChunkTaskResult record | **stub** | 無直接 struct | 整合在 builder 中 |
| TileWorldRenderer3D.cs | RebuildDirtyMeshes() / ApplyTaskResult() / BuildMeshDataOffThread() / FaceVerts() / L3() / V3() | 已實作 | `AVoxelWorldActor::Tick`；`GreedyMesher.cpp` | Greedy Mesh 演算完整移植 |
| TileWorldRenderer3D.cs | PrecomputeBorderAir() | **stub** | `GreedyMesher.cpp` L18-95 | Godot 快照式 → C++ 實時查詢 |
| TileWorldRenderer3D.cs | SetProjectiles / UpdateHighlightMesh / EnsureHighlightMeshNode / SameHighlightTiles | **完全缺失** | — | Gameplay 層 / 採掘 UI 另行實作 |
| CaGpuSimulator.cs | 整個 GPU CA 模擬器（AW/AH/AD / IsAvailable / _rd / _shader / Pack/Unpack / Initialize / Upload / Simulate / Download / Dispose / ShaderGlsl）| **完全缺失** | — | M-10 計畫，未實作 |
| MaterialData.cs | FMaterialData（Physics / bFlammable / Density / BurnMin/BurnMax）| 已實作 | `MaterialRegistry.h` L15-22 | 完整對應 |
| MaterialData.cs | DisplayName / BaseColor / IsMineable / Hardness / RequiredToolTier / BlastResistance / MagicResistance / DefaultDrops / FragmentItem / NativeElement / Opacity | **完全缺失** | — | 遊戲機制層（採掘 / Loot）待實作 |
| MaterialData.cs | IsTransparent | **stub** | `GreedyMesher.cpp` L13-15 | 簡化版：按 Material ID 判斷 opaque/transparent |
| MaterialRegistry.cs | _data[] / Register() / Get() | 已實作 | `MaterialRegistry.cpp` | 完整對應 |
| MaterialRegistry.cs | GetColor() | **完全缺失** | — | UE5 使用 Material Instance，非色表 |
| MaterialType.cs | EMaterialType enum（Air / Stone / Dirt / Wood / Sand / Water / Lava / Fire / Steam / Ash / CoalOre / CopperOre / IronOre / MagicCrystalOre）| 已實作 | `MaterialType.h` L20-41 | 已實作但 ID 編號有調整（Wood 3→7，Sand 5→4，Water 6→5，Fire 7→11，IronOre 12→9）|
| MaterialType.cs | PhysicsCategory enum（Empty / Static / Powder / Liquid / Gas）| 已實作 | `MaterialRegistry.h` L6-13 | 完整對應 |
| PlacedObjectRegistry.cs | PlacedObjectRegistry + _tileToUnit / _units / Register / TryGetUnit / NotifyDestroyed / RemoveUnit / Save/Load | **完全缺失** | — | Gameplay 物件管理層 |
| PlacedUnit.cs | PlacedUnit class（Id / Mat / Tiles / Damage / IsIntact）| **完全缺失** | — | Gameplay 物件層 |
| PlacementShape.cs | PlacementShape enum + ShapeVoxels::GetOffsets() | **完全缺失** | — | 遊戲編輯器功能 |
| PlacementValidator.cs | CanPlace() | **完全缺失** | — | Gameplay 驗證邏輯 |
| SurfaceWaterPool.cs | 整個地表水池地形特徵（Pool / _pools / Initialize / Prepare / PlaceInWorld / GetSurfaceOverride / BowlSurface / PlaceTile）| **完全缺失** | — | W 系列地形特徵 |
| TerrainFeature.cs | 抽象基底類別（Name / Initialize / Prepare / PlaceInWorld / GetSurfaceOverride）| **完全缺失** | — | W 系列地形系統 |
| SkyConfig.cs | TopColor / HorizonColor / CloudCoverage / CloudSpeed / CloudColor / Brightness | **完全缺失** | — | 天空視覺化 |
| SkyController.cs | SkyController（Config / _mat / Initialize / _Process / PushConfig / SkyShaderSrc）| **完全缺失** | — | 天空控制器 |

### 架構差異摘要

- Greedy Meshing 核心渲染完整移植（M-6 完成）
- MaterialRegistry / MaterialType 完整遷移（ID 編號有調整需注意）
- GPU CA 模擬（CaGpuSimulator）為 M-10 計畫，完全未實作
- 地形特徵系統（SurfaceWaterPool / TerrainFeature）、天空系統、採掘 / Loot 系統全部缺失
- Material Type ID 不一致需要在存檔相容時處理

---

## 8. Enemy AI（Godot 行數 971 行）

### 涵蓋 Godot 檔案

| 檔案名稱 | 行數 |
|---------|------|
| Enemy.cs | 430 |
| EnemyManager.cs | 99 |
| EnemyProjectile.cs | 51 |
| MobSpawnController.cs | 153 |
| CameraController.cs | 238 |
| **總計** | **971** |

### 逐項對照表

| Godot 檔案 | 項目名稱 | UE5 狀態 | UE5 對應位置 | 備註 |
|-----------|---------|---------|------------|------|
| Enemy.cs | EEnemyState enum（Idle / Chase / Attack）| 已實作 | `AEnemy.h` L20-26 | 完整對應 |
| Enemy.cs | EEnemyType enum（Melee / Ranged / Patrol / Heavy）| 已實作 | `AEnemy.h` L11-18 | 完整對應 |
| Enemy.cs | UniqueId / NextId static / Position / SpawnGridPos / Type / Hp / MaxHp / IsAlive / Category / AIState / Aura | 已實作 | `AEnemy.h` L53-108 | UPROPERTY 對應 |
| Enemy.cs | IElementalTarget.EntityId / TakeDirectDamage() | 已實作 | `AEnemy.h` L100-101 | 介面實現完整 |
| Enemy.cs | WandsToFire / FacingX / FacingZ | **完全缺失** | — | BT 架構中由 BT Task 直接處理 |
| Enemy.cs | RespawnTime / Gravity / MaxFallSpeed / PatrolRange 常數 | **完全缺失** | — | 復活由 MobSpawnController 管理；重力 M-6+ |
| Enemy.cs | BaseMoveInterval / MoveInterval / AttackInterval / AttackDamage / AttackRange / DetectRange / XpReward | 已實作 | `AEnemy.cpp` L52-117 | 改為 public 方法 |
| Enemy.cs | Constructor / ForceDespawn() / Respawn() | 已實作 | `AEnemy.cpp` L7-50 | 完整對應 |
| Enemy.cs | StartRespawn() / TickRespawn() / ForceRevive() | **完全缺失** | — | 復活計時器在 MobSpawnController |
| Enemy.cs | TakeSnapshot() / RestoreFromSnapshot() | **完全缺失** | — | 快照 M-7+ |
| Enemy.cs | Update() / UpdateMelee / UpdateRanged / UpdatePatrol / UpdateHeavy | **stub** | BT 架構分散 | 邏輯由 BT Tasks 取代 |
| Enemy.cs | TryMoveXZ() | 已實作 | `UBTTask_MoveOnGrid.cpp` L16-113 | 改為 BFS 尋路 |
| Enemy.cs | ApplyGravity() | **完全缺失** | — | 重力系統 M-4 未實作 |
| Enemy.cs | TakeDamageAmount() | 已實作 | `AEnemy.cpp` L37-41 | 直接 Aura modifier 應用 |
| EnemyManager.cs | Enemies TArray / DynamicActiveCount | 已實作 | `AEnemyManager.h` L38-39 | 完整對應 |
| EnemyManager.cs | EnemyProjectiles / _spawner / FireDps / LavaDps / BoltDamage 常數 | **完全缺失** | — | M-5 待實作 |
| EnemyManager.cs | SetSpawner() | **完全缺失** | — | M-5 待實作 |
| EnemyManager.cs | Spawn() | 已實作 | `AEnemyManager.cpp` L10-29 | 建立 AEnemy Actor |
| EnemyManager.cs | Update() / Tick() | **stub** | `AEnemyManager.cpp` L31-61 | 只清理死亡敵人；生成邏輯 M-5 |
| EnemyManager.cs | ApplyExplosionDamage() | 已實作 | `AEnemyManager.cpp` L63-73 | 球形傷害（曼哈頓→歐式距離）|
| EnemyProjectile.cs | Position / _dir / _damage / _moveTimer / TilesTravelled / MoveInterval / MaxRange | 已實作 | `ASpellProjectile.h` | 完整對應 |
| EnemyProjectile.cs | Constructor / Init() / Update()→Tick() / AdvanceOneTile() / FindEnemyAt() | 已實作 | `ASpellProjectile.cpp` | 完整對應 |
| EnemyProjectile.cs | IsAlive flag | **完全缺失** | — | 改為 IsValid(this) |
| MobSpawnController.cs | 全部（MinSpawnDist / MaxSpawnDist / DespawnHardDist / MaxCommonActive / BaseInterval / SpawnRateMultiplier / EnsureChunkAt / GetTerrainY / Update / HandleDespawns / PickEntry / TryFindSpawnPos / HorizDist）| **完全缺失** | — | 整個 MobSpawnController M-5 待實作 |
| CameraController.cs | 全部（CameraMode enum / ThirdPerson / FirstPerson / Isometric / SideScroll2D / TpsArmLength / _Process / ProjectScreenToWorld / CycleMode 等）| **完全缺失** | — | M-7+ Phase 2-C；UE5 使用 APlayerController + Spring Arm |

### 架構差異摘要

- AEnemy 核心屬性與 BT 介面完成；主要邏輯由 BT Tasks 取代 Godot Update()
- AEnemyManager.Spawn() + 爆炸傷害已實作；動態生成邏輯 M-5
- ASpellProjectile 投射物基礎框架完成
- MobSpawnController 整體完全缺失（M-5）
- CameraController 整體完全缺失（M-7+）

---

## 9. Items / Equipment / Inventory（Godot 行數 444 行）

### 涵蓋 Godot 檔案

| 檔案名稱 | 行數 |
|---------|------|
| PlayerEquipment.cs | 64 |
| DroppedItem.cs | 36 |
| DroppedItemManager.cs | 101 |
| EquipmentSlotType.cs | 3 |
| Inventory.cs | 91 |
| ItemData.cs | 19 |
| ItemDrop.cs | 14 |
| ItemId.cs | 40 |
| ItemRegistry.cs | 62 |
| ItemStack.cs | 14 |
| **總計** | **444** |

### 逐項對照表

| Godot 檔案 | 項目名稱 | UE5 狀態 | UE5 對應位置 | 備註 |
|-----------|---------|---------|------------|------|
| EquipmentSlotType.cs | EquipmentSlotType enum（None / Weapon / Armor / Accessory）| **完全缺失** | N/A | |
| ItemStack.cs | ItemStack struct（Empty / ItemId / Count / IsEmpty / WithCount）| **完全缺失** | N/A | |
| ItemId.cs | ItemId enum（None / BlockDirt / BlockStone / BlockWood / BlockSand / BlockAsh / ToolBasicPick / ToolBasicAxe / ToolIronPick / EquipBasicSword / EquipLeatherArmor / EquipAmulet / OreCoal / OreCopperRaw / OreIronRaw / OreMagicCrystal / Fragment* 系列）| **完全缺失** | N/A | |
| ItemDrop.cs | ItemDrop struct（ItemId / MinCount / MaxCount / Chance / 建構子）| **完全缺失** | N/A | |
| ItemData.cs | ItemData record（Id / DisplayName / IsPlaceable / PlaceAs / IsTool / ToolTier / MiningSpeedMult / MaxStack / EquipSlot / AtkMult / DefFlat / MpBonus）| **完全缺失** | N/A | |
| ItemRegistry.cs | ItemRegistry static class（_data / 所有物品 Register 呼叫 / Reg() / Get()）| **完全缺失** | N/A | |
| Inventory.cs | Inventory class（HotbarSize=10 / TotalSize=30 / Slots / ActiveHotbarIndex / ActiveItem / TryAdd / Consume / ActiveToolTier / SwapSlots / ActiveMiningSpeedMult）| **完全缺失** | N/A | |
| DroppedItem.cs | DroppedItem class（Position / Stack / LifeTime / IsAlive / _fallTimer / FallInterval / Update 含重力邏輯）| **完全缺失** | N/A | |
| DroppedItemManager.cs | DroppedItemManager class（_items / Spawn / SpawnFragments / Update 含自動拾取 / 裝備自動穿戴邏輯）| **完全缺失** | N/A | |
| PlayerEquipment.cs | PlayerEquipment sealed class（WeaponId / ArmorId / AccessoryId / TotalAtkMult / TotalDefFlat / TotalMpBonus / TryEquip / TryUnequip）| **完全缺失** | N/A | |

### 架構差異摘要

- 整個物品/背包/裝備系統在 UE5 中完全未實作（0%）
- 建議 UE5 對應架構：`EItemId` UENUM / `FItemData` USTRUCT / `UItemRegistry` Singleton / `FItemStack` USTRUCT / `UInventoryComponent` / `UEquipmentComponent` / `ADroppedItemActor` / `UDroppedItemManager`
- 優先級：M-5 ItemData+ItemId → M-6 Inventory+Equipment → M-7 DroppedItem 重力

---

## 10. GameFlow / Snapshot / Save（Godot 行數 1,030 行）

### 涵蓋 Godot 檔案

| 檔案名稱 | 行數 |
|---------|------|
| CharacterSaveData.cs | 26 |
| FlowSaveSystem.cs | 88 |
| WorldSaveData.cs | 16 |
| GameFlowUI.cs | 503 |
| ISnapshottable.cs | 8 |
| AuraSnapshot.cs | 22 |
| CharStateSnapshot.cs | 25 |
| CharStatsSnapshot.cs | 223 |
| EntitySnapshot.cs | 24 |
| SnapshotManager.cs | 82 |
| TileWorldSnapshot.cs | 13 |
| **總計** | **1,030** |

### 逐項對照表

| Godot 檔案 | 項目名稱 | UE5 狀態 | UE5 對應位置 | 備註 |
|-----------|---------|---------|------------|------|
| CharacterSaveData.cs | CharacterSaveData class（Name / Hp / Mp / SpellGroupJson）| 已實作 | `FCharacterSaveData` | USTRUCT；SpellBook 改為 TArray<FSpellArray> |
| CharacterSaveData.cs | Id 自動生成 | 已實作 | `FCharacterSaveData` | UE5 版本暫未自動生成，設計改為外部管理 |
| CharacterSaveData.cs | Level / Xp / InventorySlots / ActiveHotbar / ManaCurrents / SlotRecord | **完全缺失** | — | 等級系統 M-7+；物品欄 M-7；多類型 MP M-6 |
| FlowSaveSystem.cs | FlowSaveSystem（MakeWorldDir()→WorldRoot()）| 已實作 | `FFlowSaveSystem` | UE5 版本簡化，無名稱版本化 |
| FlowSaveSystem.cs | SavePath | **stub** | — | 無實作，需改用 ProjectSavedDir() |
| FlowSaveSystem.cs | Dto 私有內嵌類 | **stub** | — | 使用 FJson 序列化，未建立對應 DTO |
| FlowSaveSystem.cs | Load() / Save() / SaveCharacter() / SaveWorld() / DeleteWorld() / DeleteCharacter() | **完全缺失** | — | 全局列表加載/存儲未實作 |
| WorldSaveData.cs | WorldSaveData（Id / Name / Seed / WorldDir / PlayerSpawn→FIntVector / bIsFirstEnter）| 已實作 | `FWorldSaveData` | USTRUCT；SpawnX/Y/Z 合併為 FIntVector |
| WorldSaveData.cs | LastPlayed | **完全缺失** | — | 最後遊玩時間追蹤未實作 |
| GameFlowUI.cs | 整個 GameFlowUI（角色選擇 / 角色創建 / 世界選擇 / 世界創建 / 確認對話框 / RebuildCharList / MakeCharCard / RebuildWorldList / MakeWorldCard 等所有 UI 方法）| **完全缺失** | — | M-8 UMG Widget 階段實作 |
| ISnapshottable.cs | interface ISnapshottable（TakeSnapshot() / RestoreFromSnapshot()）| **完全缺失** | — | M-9 Rollback 機制 |
| AuraSnapshot.cs | AuraSnapshot record（Auras / Effects）+ AuraEntryData + AuraEffectData | **完全缺失** | — | M-9 Rollback |
| CharStateSnapshot.cs | CharStateSnapshot record（Stamina / MentalEnergy / Mood / BodyTemperature / Thirst / Hunger / Oxygen / From()）| **完全缺失** | — | M-9 Rollback |
| CharStatsSnapshot.cs | CharStatsSnapshot record（全部 65 個 CharacterStats 屬性快照 + ElemAffinity/OutputMult/Resistance 字典 + ApplyTo() / From()）| **完全缺失** | — | M-9 Rollback |
| EntitySnapshot.cs | EntitySnapshot record（EntityId / Position / Hp / Mp / WasAlive / Aura / CharState / CharStats / PlayerId=-1 常數）| **完全缺失** | — | M-9 Rollback |
| SnapshotManager.cs | SnapshotManager static class（WorldSnapshot（AnchorTimestamp / Entities / Tiles）/ _stack / StackDepth / TakeSnapshot() / ApplyLatest() / Clear()）| **完全缺失** | — | M-9 Rollback |
| TileWorldSnapshot.cs | TileWorldSnapshot record（Center / Radius）| **完全缺失** | — | M-9 Rollback |
| TileWorldSnapshot.cs | Cells 字典 | 已實作 | `FTileWorld3D::SnapshotRegion/RestoreRegion()` | 改用線性 TArray<FTileCell>；無 record 包裝 |

### 架構差異摘要

- 存檔基礎結構已移植（FCharacterSaveData / FWorldSaveData / FlowSaveSystem）
- 全局列表加載/存儲（Load/Save）及刪除邏輯未實作（M-7+）
- GameFlow UI 整體缺失（M-8 UMG 階段）
- 快照系統全部缺失（M-9）：ISnapshottable / SnapshotManager / EntitySnapshot / CharStatsSnapshot / AuraSnapshot 等
- 僅 FTileWorld3D::SnapshotRegion/RestoreRegion() 有基礎實作

---

## 11. UI / Main / Input（Godot 行數 7,053 行）

### 涵蓋 Godot 檔案

| 檔案名稱 | 行數 |
|---------|------|
| Main.cs | 2,970 |
| InputBindings.cs | 181 |
| PlayerController.cs | 473 |
| AbilityEditorUI.cs | 1,562 |
| BlockScript.cs | 370 |
| EditorSettings.cs | 9 |
| ScratchCanvas.cs | 1,074 |
| ScriptCanvas.cs | 527 |
| SpellDescriptionGenerator.cs | 164 |
| SpellListUI.cs | 455 |
| TotemLibrary.cs | 268 |
| **總計** | **7,053** |

### 逐項對照表

| Godot 檔案 | 項目名稱 | UE5 狀態 | UE5 對應位置 | 備註 |
|-----------|---------|---------|------------|------|
| Main.cs | _renderer3d / _world3d / _camera3d / _mapGen / _sky / _spellList | **完全缺失** | — | UE5 各系統由對應 Plugin 管理 |
| Main.cs | _editor（AbilityEditorUI）| **stub** | `SBlockEditorWidget` / `UBlockEditorWidget.h` | UI 框架存在但邏輯不完整 |
| Main.cs | _player（PlayerController）| 已實作 | `ASkillCreatorCharacter.h / ASkillCreatorPlayerController.h` | 角色控制系統 |
| Main.cs | _Ready() / StartGameplay() / _Process() / _Input() / SaveAll() | **stub** | `ASkillCreatorGameMode::StartGame()` | 初始化分散；HUD 刷新邏輯缺失 |
| Main.cs | HpLabel 更新 | **stub** | `UPlayerHUDWidget.h` | HUD 更新分散 |
| Main.cs | 技能欄位顯示 / 傷害數字池 | **完全缺失** | — | Floating damage text 系統無 |
| InputBindings.cs | RegisterAll() | **stub** | `ASkillCreatorPlayerController::SetupInputBindings()` | UE5 用 Enhanced Input System |
| InputBindings.cs | GetKeys() / Rebind() / ResetToDefault() / SaveToFile() / LoadFromFile() | **完全缺失** | — | 運行時鍵位配置無 |
| PlayerController.cs | IElementalTarget 實作 | **stub** | `ASkillCreatorCharacter.h` | 介面存在但方法可能不完整 |
| PlayerController.cs | Aura / Stats / State / Position / Facing / Inventory / Equipment / Level / Xp / TierName / ActiveManaSlots / Mp / MaxMp | 已實作 | `ASkillCreatorCharacter.h` | 完整對應 |
| PlayerController.cs | TakeDamage() / UpdateEnvironment() / Tick() / TryMove() / TryMoveDir() / TryMoveDepth() / ApplyPhysics() / IsOnGround() / StartJump() / Respawn() / TickMining() / GainXp() | 已實作 | `ASkillCreatorCharacter.cpp` | 完整對應 |
| PlayerController.cs | SetCastCooldown() | **stub** | `ASkillCreatorCharacter::SetCastCooldown()` | 呼叫點可能不完整 |
| PlayerController.cs | Snapshot 系統（TakeSnapshot() / RestoreFromSnapshot()）| **stub** | `ASkillCreatorCharacter` 對應方法 | S-7 詳細需驗證 |
| AbilityEditorUI.cs | SpellGroup / Loadout（當前組）| **stub** | `UBlockEdGraphNode.h` 或對應資料結構 | 技能 Loadout 不完整 |
| AbilityEditorUI.cs | _Ready() / BuildUI() / InitSpells() / GetSpellGroupJson() / SaveSpell() | **stub** | `SBlockEditorWidget::Construct/LoadSpells` | 框架存在但邏輯不完整 |
| AbilityEditorUI.cs | SwitchEditorGroup() / 容器效果編輯 | **完全缺失** | — | 編輯器內技能組切換無 |
| AbilityEditorUI.cs | 技能因子/積木/刻印庫 UI | **stub** | `SBlockEditorWidget` | UI 面板結構存在但內容不完整 |
| BlockScript.cs | BlockScript 類 / _Ready() / Rebuild() | **stub** | `UBlockEdGraphNode.h` | 卡片重建邏輯簡化 |
| BlockScript.cs | 分支容器渲染 / SplitAt() / AppendBlocks() | **完全缺失** | — | If / Loop 分支視覺化無 |
| EditorSettings.cs | AutoInsertBaseEngraving | **stub** | Plugin Settings | 設定框架存在但功能不完整 |
| ScratchCanvas.cs | SyncFrom() / Scratch 式積木展示 / 拖拉機制（BlockDrag）| **stub** | `SBlockEditorWidget` | 視覺化框架存在但細節不完整 |
| ScratchCanvas.cs | Changed 事件 | **完全缺失** | — | 變更通知系統無 |
| ScriptCanvas.cs | ScriptCanvas 類 / SyncFrom() | **stub** | `SBlockEditorWidget` | 自由畫布模式簡化 |
| ScriptCanvas.cs | 拖拉重排邏輯 / 磁吸連結 / 縮放平移 | **完全缺失** | — | 自由畫布互動無 |
| SpellDescriptionGenerator.cs | GenerateStructured() / GenerateProse() | **完全缺失** | — | 技能描述生成 UI 無 |
| SpellListUI.cs | SpellListUI class / 技能圓球渲染 / Tooltip / 技能組圓點 | **完全缺失** | — | 圓球列表首頁 UI 整體無 |
| TotemLibrary.cs | AllTotems（27 種）/ AllEngravings（90 種）/ ContainerActionIds / DefaultActionEngraveId | **stub** | `UBlockEdGraphNode.h` 或資料表 | 資料結構存在但內容不完整（許多 TODO-STUB）|

### 架構差異摘要

- PlayerController（ASkillCreatorCharacter）核心邏輯（移動 / 傷害 / 等級）已完整實作
- 積木編輯器框架（SBlockEditorWidget）已建立，但大部分互動邏輯不完整
- 自由畫布（ScriptCanvas）完全缺失；Scratch 式（ScratchCanvas）邏輯簡化
- 技能列表 & 圓球 UI（SpellListUI）完全無對應實作
- Totem & Engrave 資料結構存在但內容不完整
- InputBindings 運行時重綁鍵位無
