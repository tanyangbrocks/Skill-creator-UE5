# issues.md 全項稽核 Round 3 — 臨時交接文件

**建立時間**：2026-06-19  
**目標**：對照 Godot 原始碼（C:\skill-creator\Scripts\）逐一核實 issues.md 所有「已實作」標記是否真正正確實作，並修復不符的地方。

> **下個 AI 接手規則**：直接從「各 Agent 任務進度」表格找尚未完成的任務繼續，不需要重讀歷史。

---

## 背景說明

過去的稽核都沒有實際讀 Godot 原始碼，而是憑記憶/計畫文件標記。  
本次每一條「已實作」都要用 Godot .cs 行號佐證。

**Godot 原始碼根目錄**：`C:\skill-creator\Scripts\`  
**UE5 原始碼根目錄**：`C:\SkillCreatorUE5\`

---

## 各 Agent 任務進度

| # | 負責 Section | Godot 主要檔案 | 狀態 | 重要發現摘要 |
|---|-------------|--------------|------|------------|
| A | §1 VM Core（ExecutionLoop opcodes + SpellCompiler cases） | AbilitySystem/VM/*.cs | ✅ 已完成（30項：✅21/⚠️7/❌2） | RepeatWhile 安全熔斷上限 UE5=1000 vs Godot=500；WaitCondition 無代理時 fallback 行為相反 |
| B | §2 SpellCaster 執行方法（act_* / Execute*） | AbilitySystem/SpellCaster.cs | ✅ 已完成（20項：✅6/⚠️8/❌6） | act_area_fan/beam 形狀公式完全不同；ExecuteTechnique 缺 Multi 迴圈；ExecuteMorph/ExecuteDomain 未依 TotemId 分支；ExecuteSummon 純空函式 |
| C | §2 SpellRunner / ActionBus / SafetyGuard / EventBus / AbilityPointCalculator | SpellRunner.cs / ActionBus.cs / SafetyGuard.cs / AbilityPointCalculator.cs | ✅ 已完成（11項：✅5/⚠️4/❌2） | 敵人傷害完全未走 ActionBus 攔截管線；EventBus 跨技能廣播物理上不可能（per-cast 局部 TSet） |
| D | §3 AbilitySystem 資料型別（ManaSlot/Type/Registry, SpellArray/Group/Loadout/Slot, EngraveData, SaveSystem） | AbilitySystem/Data/*.cs / SaveSystem.cs | ✅ 已完成（14項：✅9/⚠️2/❌3） | EngraveData.CalculateEffect() 寫死線性公式，未實作 Hyperbolic/Linear 雙公式；SaveSystem 缺未知 ManaTypeKey 防禦清除；ContainerType 三種召喚容器被合併 |
| E | §4 Elemental System（IElementalTarget, AuraComponent, ReactionTable, StatusEffect 5 種） | AbilitySystem/Elemental/*.cs | ✅ 已完成（19項：✅19/⚠️0/❌0） | 全部正確對應，本次稽核還原度最高的模組；唯一發現是文件落後（TakeSnapshot 已實作但文件仍寫暫緩） |
| F | §5 Character/Combat/Interfaces（CharacterStats, CharacterState, CombatState, ICreature, IWorldInterface, GridPos/3D, WorldScale, GameClock） | Characters/*.cs / World/GridPos*.cs / World/WorldScale.cs | ✅ 已完成（12項：✅6/⚠️4/❌2） | IWorldInterface.GetEntitiesNear 註解誤標曼哈頓距離（實際應歐幾里德）；缺 GetEntityProperty/SetEntityProperty/CreateEntity 三方法 |
| G | §6 Voxel World Core（TileCell, TileWorld3D CA 物理, Chunk3D, MapGenerator3D） | World/TileWorld*.cs / World/MapGenerator*.cs / World/Chunk3D.cs | ✅ 已完成（12項：✅4/⚠️5/❌3） | Lava 限速缺失+點火機率差20倍；GetHeightAt振幅差4倍+基準偏10%；洞穴演算法CA平滑→noise閾值根本差異 |
| H | §7 Rendering/Materials/Sky（GreedyMesh, MaterialData/Registry, PlacedObjectRegistry, TerrainFeature, SkyController） | Rendering/*.cs / Materials/*.cs | ✅ 已完成（10項：✅3/⚠️4/❌3） | Variant 色差機制完全缺失；GreedyMesher 無半透明雙 pass；Hardness 單位制不符；顏色數值整體偏移 |
| I | §8 Enemy AI（Enemy, EnemyManager, EnemyProjectile, MobSpawnController, CameraController） | AI/*.cs / Enemy.cs / EnemyManager.cs | ✅ 已完成（14項：✅10/⚠️3/❌2） | 四種 AI 行為分支完全缺失（BT_Enemy 不存在）；EnemyManager FireDps/LavaDps/BoltDamage 數值偏低且缺 XP 發放 |
| J | §9 Items/Equipment/Inventory + §10 GameFlow/Snapshot/Save | Items/*.cs / Snapshot/*.cs / FlowSaveSystem.cs | ✅ 已完成（21項：✅12/⚠️5/❌3） | 礦石物品 IsPlaceable 語意錯誤；DroppedItem 缺重力落下＆碎片批次生成；GameFlowWidget 缺世界預先地形生成 |
| K | §11 UI/Main/Input（Main.cs 採掘/放置/鍵位, InputBindings, PlayerController 移動物理, AbilityEditorUI 儲存/驗證） | Main.cs / InputBindings.cs / PlayerController.cs / AbilityEditorUI.cs | ✅ 已完成（24項：✅10/⚠️8/❌6） | 積木編輯器 UI 嚴重缺失（無左側 Palette/右側統計面板/5組切換/容器導覽）；F1 畫筆工具未實作；X-Ray 功能未移植；採掘/放置邏輯核心正確 |

---

## 稽核重點說明（給每個 Agent）

### 核實方法
1. **找 Godot 對應 .cs 行號** → 讀取邏輯（公式、預設值、條件）
2. **找 UE5 對應 .h/.cpp** → 讀取實際實作
3. **比對是否一致**（數值、公式、欄位、邏輯分支）
4. **判定**：
   - ✅ 正確對應（可能有合理架構差異，但設計意圖一致）
   - ⚠️ 部分不符（有差異但不嚴重）
   - ❌ 錯誤/缺失（issues.md 標「已實作」但實際不符或根本沒有）

### 特別注意
- issues.md 裡標「已實作」的都要懷疑，不要只核實「stub/缺失」的
- 架構差異（例如 Godot 用 Dictionary C# → UE5 用 TMap，邏輯等價）視為正確
- 但若公式數值不同、邏輯分支缺失、預設值錯誤，則標為不符

---

## 已確認修復項（本 session 前段已處理）

| 項目 | Godot 依據 | 修復內容 | 狀態 |
|-----|-----------|---------|------|
| bHoldToPlace 預設值 | Main.cs:124 `_holdToPlace = true` | ASkillCreatorHUD.h false→true | ✅ 已修復 |
| PlaceCooldown 時機 | Main.cs _process 每幀倒數 | ASkillCreatorCharacter.cpp 移至 Tick() | ✅ 已修復 |
| GridPos.DistanceTo() | GridPos.cs L14-18 Euclidean | GridPos.h EuclideanDistance() 已正確 | ✅ 確認正確 |
| EnemyManager.Tick | EnemyManager.cs Update() | AEnemyManager.cpp 有環境傷害+死亡清理 | ✅ 確認正確 |
| EventBus 架構 | EventBus.cs 全域靜態 | UE5 per-cast 局部（架構差異，記錄在案） | 📝 記錄缺口 |

---

## Agent 詳細發現

> 以下各節由對應 Agent 完成後填入。

> **完整原始資料**：每個 Agent 的全部 findings（含未列出的 ✅ 項目逐項佐證）保存在 `docs/audit-round3-results/agent-{A~F}.json`。下方只列出 ⚠️/❌ 項目的完整細節（需要動手修的部分），✅ 項目僅列一行摘要；若本節內容被後續 compact 摘要化，請回頭讀 JSON 檔案還原細節，不要憑記憶重寫。

### Agent A — §1 VM Core
**核實 30 項：✅21 / ⚠️7 / ❌2**（JSON：`agent-A.json`）

✅ 正確對應（21項，僅列摘要）：EBlockType/EOpCode 完整性、If/Evaluate/RepeatN/RepeatWhile/ForEachNearby 編譯位移、RandomChoice/AlternateTrigger/SinglePulse/TaskCounterOnReach/EdgeRising/EdgeFalling/Wait/Sleep/ListGet/ListSet/ListRemoveAt/ListDequeue/ListPop/ListContains/Die/Discard/EffectLabel/Engraving/default case/QueryNearest/RandomJump 執行邏輯，皆與 Godot 逐行一致。

❌ A-3｜**RepeatWhile 安全熔斷上限不一致**：Godot `SafetyGuard.MaxWhileIterations=500`（SafetyGuard.cs:10），UE5 `ExecutionLoop.h:16` 卻自訂獨立常數 `MaxWhileLoopIterations=1000`（註解誤導性宣稱「同 SafetyGuard 但獨立計數」，但常數值本身也不同），導致 UE5 容忍條件恆真迴圈跑 2 倍次數才 Fizzle。
→ 修法：`ExecutionLoop.h` 的 `MaxWhileLoopIterations` 改為直接引用 `FSafetyGuard::MaxWhileIterations`。

❌ A-4｜**WaitCondition 無代理時 fallback 行為相反**：Godot（ExecutionLoop.cs:59-65）在無 `PlayerStatsQuery` 代理時 `met=true` 直接放行；UE5（ExecutionLoop.cpp:161-167）改成 `bMet=false` 永久卡住並打警告。同步測試/缺少代理注入的施放路徑會卡死在 WaitingCondition。
→ 修法：若非刻意變更，改回 `bMet=true`；若刻意，需在 issues.md 明確記錄為「有意偏離 Godot」。

⚠️ 部分不符（7項，需留意但非阻斷性）：A-13 Compare 全角符號 vs ASCII（'=' / '≠' 字串不匹配風險）、A-14 Totem 查表機制改為直接攜帶 SlotRef（Tsm 參數已是擺設）、A-15 GetVar/GetVarBool 正確性依賴 M-9 編輯器填值規則、A-22 VecCross 2D→3D 升級（純量→向量，下游讀取方式需注意）、A-24 ResolveNum 動態型別→FNumRef 強型別（已知架構轉換）、A-25 RegisterFilter 的 cancel/halve/cap 實際邏輯不在 VM 層（需跨 Agent C 確認，**已由 C-8 確認 ✅ 邏輯正確**）、A-29 ExecutionContext 缺 `EffectOriginOverride` 對應欄位（需跨 Agent B 確認 ForEachNearby 效果原點覆寫機制）。

### Agent B — §2 SpellCaster 執行
**核實 20 項：✅6 / ⚠️8 / ❌6**（JSON：`agent-B.json`）

✅ 正確對應（6項，僅列摘要）：TryCast 主流程、act_area_around 半徑公式、ReadMods 刻印讀取、ApplyGlobalEngravings（green_death_replace/green_invincible，先前文件誤判為缺失）、ExecuteEffects→FSpellRunner 架構重構、VM 執行迴圈整體骨架。

❌ B-5｜**SummonMinion/Turret/Guardian 容器無顯式 case**：落到隱性 DirectCast fallback（行為碰巧等同 Godot 的已知 stub，但 UE5 端 `ExecuteSummon()` 是純 `UE_LOG` 空函式，連視覺占位都沒有，比 Godot 更倒退）。
→ 修法：補上 `SpawnEffectTiles` 視覺占位效果（對應 SpellCaster.cs:879-891），並顯式列出三個 case 加註解標明技術債。

❌ B-7｜**act_area_fan 形狀公式嚴重不符**：Godot 寬度 `spread=(d+1)/2`（緩慢展開）、僅 Stone 擋格、整排都擋住才中斷整個 fan。UE5 寬度 `w=-d..d`（展開速度 2 倍）、任何非 Air 都擋格、外層 d 迴圈完全沒有依整排阻擋中斷。
→ 修法：寬度改 `spread=(d+1)/2`；阻擋判定改回只看 `EMaterialType::Stone`；補上整排阻擋才中斷外層迴圈的邏輯。

❌ B-9｜**act_area_beam 缺正交寬度 + 長度公式錯誤**：Godot 三格寬光束（正交 s=-1..1）、`beamLen=18+baseR`（隨 DmgBonus 變化）、僅中心格是 Stone 才中斷。UE5 單格寬、`BeamRange*Grain=192` 固定超長（與 baseR/DmgBonus 脫鉤）、任何非 Air 都中斷。
→ 修法：補正交寬度迴圈；長度改 `18*GrainScale+BaseR`；中斷判定改回僅看中心格 Stone。

❌ B-10｜**ExecuteTechnique 缺 Multi 重複次數迴圈，各武技公式不符**：完全沒有 `for(rep=0;rep<M.Multi;rep++)` 外層迴圈（blue_multi 刻印失效）；sword/punch/shield 半徑公式與 atHitPoint offset 邏輯皆與 Godot 不同；technique_projectile/area/beam/chain 四種舊版相容分支完全缺失。
→ 修法：補 Multi 迴圈；半徑統一用 `r=2+(int)(DmgBonus*3)`；補回 4 種舊版分支。

❌ B-12｜**ExecuteMorph 未依 TotemId 分支**：Godot 依 `morph_speed/strengthen/flight/invisible` 四種分別做不同視覺效果。UE5 完全不分支，統一在 Origin 周圍塞一個 Stone 球體（地形塑形而非變幻 buff），語意完全跑偏。
→ 修法：依 `Slot.TotemId` 分支還原四種效果（對應 SpellCaster.cs:823-835）。

❌ B-14｜**ExecuteSummon 是純空函式**：只有一行 `UE_LOG`，無任何效果（已知技術債註記 W-11，但比 Godot 的視覺占位更倒退）。同 B-5 修法。

❌ B-15｜**ExecuteDomain 未依 TotemId 分支**：Godot `domain_barrier`（環狀護欄）/`domain_terrain`（純爆炸）/`domain_weather`（雨幕線）三種完全不同效果。UE5 統一簡化成單次大爆炸，遺失 barrier 結構與 weather 持續性語意。
→ 修法：依 TotemId 分三支還原。

⚠️ 部分不符（8項，較次要）：B-2 MP 多槽均分邏輯缺失（永遠扣在第一個找到的槽，未均分）、B-3 投射物起點/方向用 ActorForwardVector 取代滑鼠瞄準方向、B-4 Contact 容器命中後缺完整技能效果鏈（只做固定傷害+元素，未呼叫 Runner.Submit 觸發完整 act_* 效果）、B-8 act_area_distant 距離公式有 Grain 平方級錯誤縮放（UE5 距離是 Godot 的 7.6 倍且隨 Grain 發散）、B-11 act_fire_projectile 命中缺 AoE 與 modifier 套用、B-13 act_dash/teleport 步數不對齊（MaxSteps=240 vs Godot 固定 20）、B-17 ApplyModsToNearbyEnemies 用瞬移取代逐格碰撞檢測且 Slow/Freeze/Stun 未實作、B-20 issues.md 第15行「§2 ~97%完成」與同檔197-200行「stub」自相矛盾，本次核實證實後者更準確。
→ **文件修正**：`docs/issues.md` 第15行應訂正為更保守的描述，與197-200行統一。

### Agent C — §2 SpellRunner/ActionBus/SafetyGuard/EventBus
**核實 11 項：✅5 / ⚠️4 / ❌2**（JSON：`agent-C.json`）

✅ 正確對應（5項，僅列摘要）：SafetyGuard 全部 6 個常數數值一致、SpellRunner.Advance 跨幀骨架（Wait/PendingInvoke*/PendingEntity* 消費）、ActionBus 三種 Mode（cancel/halve/cap）邏輯、ActionBus DeltaTime 倒數計時重構、TriggerCombo callback 拆分（issues.md 先前誤判為 stub）。

❌ C-6｜**敵人傷害完全未走 ActionBus 攔截管線**：Godot `Enemy.TakeDamage`（Enemy.cs:425）透過 `ActionBus.Dispatch(EntityDamageAction)` 與玩家傷害走同一管線。UE5 `AEnemy::TakeDamageAmount`（AEnemy.cpp:56）全文無任何 ActionBus 呼叫；`FActionBus` 也只有 `DispatchPlayerDamage`/`DispatchPlayerDeath` 兩個 API，無 `DispatchEntityDamage` 對應。「對敵人的傷害護盾/反制」類效果架構上完全不可能實現。
→ 修法：`ActionBus.h` 增加 `DispatchEntityDamage(EntityId, Damage)`；`AEnemy::TakeDamageAmount` 先過濾再扣血。

❌ C-7｜**EventBus 跨技能廣播物理上不可能**：UE5 `USpellCaster.cpp:698,749-750` 每次 `BuildContextDelegates` 都建立全新 `MakeShared<TSet<FName>>`，取代 Godot 全域 `static HashSet`（EventBus.cs:6-24）。技能A 的 Broadcast 訊號物理上無法被技能B 的 OnReceive 收到（各自獨立集合），且缺少 Godot 的「每幀清空」生命週期語意——訊號從「全域以幀為單位」降級成「單次施放內部私有狀態」。這不只是架構差異記錄所說的「per-cast 局部」，是真實功能缺口。
→ 修法：在遊戲層維護跨所有施放共享的全域 `TSet<FName>`，每幀 Tick 開頭清空，`HasSignalFn`/`BroadcastFn` 指向此全域集合而非各自局部集合。

⚠️ 部分不符（4項）：C-2/C-3 issues.md 對 MaxComboDepth「8 vs Godot 5」與 TriggerCombo「stub」的記錄已過期不準確（實際程式碼皆正確，純文件問題，需訂正 issues.md:205-206）；C-5 `SpellRunner.PruneAfter` 語意從「撤銷某時間點後提交+退MP」改為「移除執行過久的條目」，且完全沒有 MP 退還機制；C-10 `AbilityPointCalculator.CalculateTotalCost` 疑似把 `LocalEngravings` 點數重複加總一次（需與 Agent D 的 D-13 交叉確認 `FSpellSlot::AbilityPointCost` 是否已內含刻印成本）。

### Agent D — §3 AbilitySystem 資料型別
**核實 14 項：✅9 / ⚠️2 / ❌3**（JSON：`agent-D.json`）

✅ 正確對應（9項，僅列摘要）：ManaTypeRegistry 18種類型完整核對、ManaSlot 欄位與Tick公式、AbilityActivationType、TotemType 9值、EngraveColor 11值、SpellArray 核心欄位與6個業務方法、HasUnboundMpBlocks 存檔驗證鏈確實接通（先前疑似未被呼叫，追加搜尋後確認有效）、SpellGroup/SpellLoadout 容量常數、Blocks JSON 序列化往返。

❌ D-4｜**ContainerType 三種召喚容器被合併且新增不存在的 Area 導致數值錯位**：Godot `SummonMinion/SummonTurret/SummonGuardian` 三個獨立值，UE5 合併成單一 `Summon`，且多了 Godot 沒有的 `Area`，與 `SummonTurret` 數值位置衝突，舊存檔 enum int 序列化會跨版本不相容。
→ 修法：補回三個獨立值，不要用單一 `Summon` 取代；新值放尾端不要插中間。

❌ D-8｜**EngraveData.CalculateEffect() 完全沒實作 Hyperbolic/Linear 雙公式**（最嚴重）：Godot 依 `ScalingType` 分支（Hyperbolic=`1-1/(1+a*x)`，Linear=`base+x*k`）。UE5 寫死單一線性公式 `Effect*(1+Points*0.1)`，且缺 `DisplayName/IsGlobal/BaseCost/RequiredPlayerLevel/Element/IsRestriction/TotalAbilityPointCost` 共7欄位。所有 Hyperbolic 縮放的刻印（傷害/機率類，占多數）強度曲線與 Godot 設計嚴重偏離。
→ 修法：補回 7 個欄位 + `ScalingCoefficient`/`BaseEffect`，`CalculateEffect()` 改為依 `Scaling` switch 出兩種公式。

❌ D-11｜**SaveSystem 缺未知 ManaTypeKey 防禦清除機制**：Godot `SaveSystem.cs:282-287` 還原時若 `ManaTypeRegistry.Get()` 找不到對應 key，會清空並警告。UE5 `FSpellSaveSystem::LoadGroupFromString()` 全文 grep 無 `FManaTypeRegistry` 引用，無效存檔資料會無聲流入遊戲邏輯。
→ 修法：反序列化後加後處理迴圈，`Find()` 失敗則 `UE_LOG` 警告並清為 `NAME_None`。

⚠️ 部分不符（2項）：D-7 `EEngraveCategory`/`EEngraveTrigger` 列舉數值錯位（Modifier/Action順序顛倒，多一個 `Other`）且缺 `OnExpire` 觸發時機；D-13 `FSpellSlot::AbilityPointCost` 疑似漏算 `LocalEngravings` 刻印成本（與 C-10 同一個問題，需交叉確認）。

### Agent E — §4 Elemental System
**核實 19 項：✅19 / ⚠️0 / ❌0** — **本次稽核還原度最高的模組**（JSON：`agent-E.json`）

全部核實項目（IElementalTarget 介面、ESkillElementType 11元素、ElementalReactionTable 22條反應逐條核對、ElementalStatusEffect 5種效果公式數值、ElementalAuraComponent 全部 Apply/Process/Recalc API、溫度偏移常數±15/-20/-8/±50、CA反應機率0.5%/3%）皆與 Godot 原始碼逐行/位元級一致，無需修復。

📝 唯一發現（非bug，文件落後）：E-17 — `實作進度.md` 第316行寫「TakeSnapshot/RestoreFromSnapshot 暫緩（M-10快照）」，但 `UElementalAuraComponent.cpp:122-170` 已完整實作（含 `CreateEffect()` 字串switch取代C#反射）。
→ 修法：更新 `實作進度.md` 移除「暫緩」字樣。

### Agent F — §5 Character/Combat/Interfaces
**核實 12 項：✅6 / ⚠️4 / ❌2**（JSON：`agent-F.json`）

✅ 正確對應（6項，僅列摘要）：CharacterStats 全欄位與初始值、CharacterState 生存四值Drain/Regen速率常數、CombatState 全部欄位與事件方法、GridPos.DistanceTo 歐幾里德公式、WorldScale Grain=16 核心派生公式（TileSizeCm/PlayerW/PlayerH/MiningRange/DefaultShapeRadius）、GameClock 常數與Advance邏輯。

❌ F-8｜**IWorldInterface.h 註解誤標距離公式**：Godot `GetEntitiesNear` 三處實作（TileWorld3D.cs:871-872 等）皆用 `GridPos.DistanceTo()` 歐幾里德距離。UE5 `IWorldInterface.h:21` 註解卻寫「曼哈頓距離」。目前零實作未造成實際bug，但未來若依此錯誤註解實作會導致偵測範圍形狀（菱形 vs 圓形）與 Godot 不同。
→ 修法：註解改為「歐幾里德距離」，未來實作用 `FGridPos::EuclideanDistance()` 而非 `ManhattanDistance()`。

❌ F-10｜**IWorldInterface 缺 GetEntityProperty/SetEntityProperty/CreateEntity 三方法**：issues.md 先前已記錄此缺失，本次逐行核對確認屬實（UE5 僅實作7/10個 Godot 方法）。整個介面目前零實作，尚未造成執行期問題，但 NPCBrain 感知系統若需要動態查詢/設定實體屬性將無管道可用。
→ 修法：補上三個方法簽名（型別需配合 UE5 慣例調整，對應 Godot `object?`/`Dictionary<string,object?>` 的彈性鍵值語意）。

⚠️ 部分不符（4項）：F-3 `UCharacterStateComponent.cpp:27-28` 多加了 Mood 高於預設值時下降的分支，Godot 只有單向回升，屬 UE5 自行擴充未對齊原版；F-7 WorldScale 世界尺寸常數設計差異（`DefaultWorldHeight=256` 為寫死值而非公式推導，與「改 Grain 全自動跟進」目標有落差，但屬刻意架構差異非疏漏）；F-9 ICreature 缺 Aura 屬性（合理的模組依賴妥協）；F-12 GridPos3D 完全未移植，合併進 FGridPos（Neighbors6 行內邏輯未逐一核對，建議由 VoxelWorld 稽核 Agent 補核）。

### Agent G — §6 Voxel World Core
**核實 12 項：✅4 / ⚠️5 / ❌3**（JSON：`agent-G.json`）

✅ 正確對應（4項，僅列摘要）：G-2 Powder 重力（垂直落下+4方向斜落，隨機起始方向，邏輯完全一致）、G-6 GetTile/SetTile 邊界檢查（無限世界 Width=0 時 InBounds=true，越界靜默回傳 Air/忽略寫入）、G-8 Chunk3D dirty tracking（IsDirty/MeshNeedsRebuild/NeedsSave + AABB DirtyMin/Max，MarkDirty/ClearDirty/ClearUpdated 語意逐行一致）、G-7 Explode 球形爆炸公式（dx²+dy²+dz²≤r² 核心正確，UE5 多 Chance 參數+30%放火為擴充功能）。

❌ G-3｜**UpdateLiquid：Lava 限速缺失 + 點火機率差 20 倍**：Godot `TileWorld3D.cs:265` 在 UpdateLiquid 入口有 `if (mat == Lava && _frame % 3 != 0) return;`，讓岩漿流速降為水的 1/3；UE5 `TileWorld3D.cpp:354` 完全無此限速，岩漿擴散速度與水相同。同函式末尾 Godot Lava 點火機率為 0.1f（TileWorld3D.cs:287），UE5 為 0.005f（TileWorld3D.cpp:380），相差 20 倍，導致 UE5 岩漿周圍燃燒效果極稀少。
→ 修法：(1) UpdateLiquid 入口加 `if (Mat==Lava && Frame%3!=0) return;`；(2) Lava UpdateLiquid 末尾點火機率改 0.1f。

❌ G-9+G-10｜**MapGenerator3D GetHeightAt 公式嚴重不符（Noise 頻率+公式雙重不符）**：Godot `MapGenerator3D.cs:70-79` 高度 noise freq=0.001f/7 octave；UE5 `MapGenerator3D.cpp:59-65` freq=0.005f/4 octave（頻率差 5 倍，octave 差 75%）。GetHeightAt 公式：Godot `worldH*0.30+n*(worldH*0.08f)`（平均地表在 30% 深度，振幅±8%）；UE5 `WorldH*0.4+v*WorldH*0.30f`（平均地表在 40% 深度，振幅±30%）。振幅差近 4 倍，地表深度差 10%（@ H=256 差 25 格），兩邊玩家看到的世界地形外觀完全不同。
→ 修法：freq 改 0.001f、octave 改 7；GetHeightAt 公式改 `WorldH*0.30f + v*(WorldH*0.08f)`，Clamp 範圍改 `(H*0.15f, H*0.45f)`。

❌ G-12｜**洞穴生成演算法根本差異：CA 平滑 vs Noise 閾值**：Godot `MapGenerator3D.cs:473-521` 對出生區做全局三步驟——隨機初始化 55% 空氣的 3D 布林陣列，再執行 4 次 Moore 鄰域 CA 平滑（threshold=18）+1 次 threshold=15，產生有機團狀大廳式洞穴。UE5 `MapGenerator3D.cpp:112-119` 完全沒有 CA 平滑，直接用 noise 閾值（a²+b²<0.02 || w>0.75），per-chunk 獨立計算，產生蜿蜒隧道式洞穴。架構選擇差異（UE5 per-chunk 更適合無限世界懶加載）可接受，但洞穴風格與 Godot 截然不同，issues.md 需明確標記此有意偏離。

⚠️ 部分不符（5項）：G-1 TileCell 缺 Variant 欄位（Godot byte 0-255 微小色差 TileCell.cs:8，UE5 無對應，多了 Category/Flags 雙軌制欄位為有意擴充）；G-4 液體水平擴散 UE5 每幀只嘗試固定方向 s=1..spread 遇阻即 break，Godot 先掃一個方向再掃反方向兩組 for，液體向後流能力略有差異；G-5 ExtinguishFire 語意差異——Godot（TileWorld3D.cs:466-481）把 Fire→Air 且所有鄰近 Water→Steam（火消失+水蒸發），UE5（TileWorld3D.cpp:464-468）直接把 Fire 格本身→Steam（未消失只換材質，鄰近 Water 不受影響）；G-5b Fire UpdateGas 點火機率 0.08f（Godot TileWorld3D.cs:314）vs 0.02f（UE5 TileWorld3D.cpp:405），差 4 倍；G-11 洞穴 noise 頻率 CaveThin 0.022→0.04（差 1.8 倍）、CaveWide 0.008→0.02（差 2.5 倍），寬洞穴門檻 0.80→0.75（UE5 洞穴稍多）。

### Agent H — §7 Rendering/Materials/Sky
**核實 10 項：✅3 / ⚠️4 / ❌3**（JSON：`agent-H.json`）

✅ 正確對應（3項，僅列摘要）：H-9 SkyConfig 欄位結構完整（6欄位對齊，CloudSpeed 單位換算合理）；H-10 SkyController 架構正確（Godot 自訂 sky shader → UE5 SkyAtmosphere 原生大氣，品質更高，TransitionTo 為 UE5 新增功能）；H-7 採掘高亮 overlay 有基礎實作（詳細縮水項目見⚠️）。

❌ H-4｜**MaterialRegistry.GetColor() 缺 variant 微小色差機制**：Godot `MaterialRegistry.cs:83-93` 的 GetColor(MaterialType, byte variant) 計算 v = (variant/255f)*0.06f - 0.03f，RGB 各加 ±3% 偏移，同一材質不同 tile 有微小色差使地形更自然。UE5 FMaterialRegistry::GetColor 無 variant 參數，所有同材質 tile 顏色完全相同。根本原因：FTileCell（MaterialType.h:46-51）無 Variant:uint8 欄位（Godot TileCell.cs:6-10 有）。
→ 修法：關 Editor + Rebuild，FTileCell 補 uint8 Variant = 0；FMaterialRegistry::GetColor 補 uint8 Variant 參數，加入 v=(Variant/255.f)*0.06f-0.03f 計算（對應 MaterialRegistry.cs:86-92）；GreedyMesher 的 ColorBuilder.Add 傳入 cell.Variant。

❌ H-5｜**Hardness 數值單位制不符 + Wood/MagicCrystalOre RequiredToolTier 偏差**：Godot Hardness 是絕對採掘幀數（Dirt=15, Sand=10, Stone=40, Coal=20, Iron=45, Magic=65，MaterialRegistry.cs:26-75 佐證）。UE5 是浮點係數（Dirt=1.f, Stone=3.f），無換算公式說明。相對大小偏差：Godot Coal(20幀) 約是 Stone(40幀) 一半，UE5 Coal=Stone=3.f 等同。RequiredToolTier 偏差：Godot Wood=0（徒手砍樹），UE5 Wood=1；Godot MagicCrystalOre=2（MaterialRegistry.cs:71-75），UE5=3。
→ 修法：MaterialRegistry.h 補換算說明（建議 1.f = 基礎工具15幀基準）；Coal Hardness 改 1.5f（Stone 一半）；Wood RequiredToolTier 改 0；MagicCrystalOre RequiredToolTier 改 2。

❌ H-6｜**GreedyMesher 缺半透明雙 pass（水/蒸汽/火無法透明渲染）**：Godot TileWorldRenderer3D.cs:64-83 建立 _matOpaque(CullMode=Disabled) + _matTransparent(AlphaBlend)；BuildMeshDataOffThread:325-326 依 MaterialRegistry.Get(fid.t).IsTransparent 分流（Water Opacity=0.55/Fire=0.65/Steam=0.35 走透明 pass，MaterialData.cs:17-23 佐證）。UE5 GreedyMesher::Build() 只輸出單一 StreamSet；FMaterialData 缺 bIsTransparent/Opacity 欄位（Godot MaterialData.cs:41-51）。Water/Steam 在 GMatColors[] 雖設 Alpha < 1（Water=0.78, Steam=0.39），但 RMC 單一 material slot 無法做 alpha 混合。
→ 修法：FMaterialData 補 bIsTransparent/Opacity 兩欄位；MaterialRegistry.cpp 填值（Water true/0.55, Fire true/0.65, Steam true/0.35）；GreedyMesher::Build 輸出透明/不透明兩個 StreamSet；AVoxelWorldActor 對透明 slot 使用 Transparency=Alpha 材質。需關 Editor + Rebuild。

⚠️ H-1｜**EMaterialType ID 重組（UE5 新增 Grass/Leaves/Ore_Gold 導致後續 ID 偏移）**：Godot 14 種（ID 0-13），UE5 新增 Grass(3)/Leaves(8)/Ore_Gold(10) 後共 17 種，Godot Wood=3 → UE5 Wood=7 等後續 ID 全部偏移。3D 擴充合理，但 Godot 存檔按 uint8 序列化跨版本讀取會材質錯亂（記錄在案，不需立即修）。

⚠️ H-2｜**FMaterialData 欄位架構差異（IsTransparent/Opacity 缺失）**：顏色/名稱分離到 GMatColors[]/GMatNames[] 查找表是合理優化；DefaultDrops 硬編碼在 switch 中（新增材質需改兩處，維護負擔）；IsTransparent/Opacity 缺失已於 H-6 詳述為 ❌ 級問題。

⚠️ H-3｜**GMatColors[] 顏色數值整體偏移（MagicCrystalOre 色相最嚴重）**：Water/Dirt/Sand 偏差小（屬設計調色選擇）。MagicCrystalOre Godot 藍綠色(0.12,0.68,0.80) → UE5 紫藍(0.60,0.40,1.00) 色相完全不同，若要對齊 Godot 設定應修正此值。

⚠️ H-8｜**TerrainFeature.GetSurfaceOverride() 缺高度覆寫能力**：Godot TerrainFeature.cs:44-48 回傳 (int h, MaterialType mat)?，可同時覆寫地表高度+材質（水池靠此讓地表 Y 降低形成積水）。UE5 TerrainFeature.h:29 只回傳 EMaterialType（Air=不覆蓋），無高度覆寫，SurfaceWaterPool 等需要地形高度下陷的特徵無法正確實作。

### Agent I — §8 Enemy AI
**核實 14 項：✅10 / ⚠️3 / ❌2**（JSON：`agent-I.json`）

✅ 正確對應（10項，僅列摘要）：EEnemyType/EEnemyState/ESpawnCategory 列舉完整性（UE5 多 Named/Boss 為合理擴充）、Enemy MaxHp 預設值（Heavy=150/Ranged=35/Patrol=45/Melee=50）、AI 時序常數（BaseMoveInterval/AttackInterval/AttackDamage/AttackRange/DetectRange 所有數值逐字一致）、MoveInterval × SpeedPenalty 乘法公式、XpReward 四種數值、Respawn/ForceDespawn/ForceRevive 生命週期骨架、EnemyManager.Spawn + DynamicActiveCount 增減邏輯、MobSpawnController 生成環六常數（MinSpawnDist/MaxSpawnDist/DespawnHard/Soft/MaxCommonActive/BaseInterval）、HandleDespawns 硬/軟 despawn 邏輯、TryFindSpawnPos + PickEntry 加權抽選。

❌ I-9｜**四種敵人 AI 行為分支完全缺失（最嚴重）**：Godot `Enemy.Update()`（Enemy.cs:171-411）直接在物件方法中實作 `UpdateMelee/UpdateRanged/UpdatePatrol/UpdateHeavy` 四種狀態機（Chase/Attack/Idle 切換、moveTimer/attackTimer 累減、TryMoveXZ 台階爬行、Ranged PreferredDist=8 反向移動、Patrol ±PatrolRange=12 巡邏、WantsToFire 遠程旗標）。UE5 採用 BehaviorTree 架構，但 `/Game/BT_Enemy` 資產不存在（AEnemyAIController.cpp:7 有警告日誌），BTTask_MoveOnGrid/BTTask_AttackPlayer 等 Task C++ 檔案也未建立。敵人目前完全沒有 AI 行為。
→ 修法（M-5/M-8 工作項）：選擇其一：(a) 建立 BT_Enemy.uasset + 最小 BTTask C++ 實作；(b) 暫時在 AEnemy::Tick() 直接還原 Godot 的 Update 邏輯作為過渡（更快速但繞開 BT 架構）。

❌ I-11｜**EnemyManager 環境傷害數值偏低 + 死亡 XP 缺失 + Bolt 生成路徑缺失**：
(1) 數值不符：FireDps Godot=25 → UE5=5（1/5 倍）；LavaDps Godot=40 → UE5=15（約 1/2.7 倍）；BoltDamage Godot=12 → UE5=8（AEnemyManager.h:20-22）。
(2) 死亡後 XP 獎勵缺失：Godot `player.GainXp(e.XpReward)`（EnemyManager.cs:61），UE5 死亡分支改成 `UCombatStateSubsystem::OnEnemyKilled()` + 掉落物，無 XP 發放。
(3) WantsToFire → EnemyProjectile 生成路徑缺失：Godot EnemyManager.cs:72-73 檢查旗標後 Spawn bolt，UE5 完全不做（依賴 I-9 AI 整體缺失）。
(4) Named/Boss 復活邏輯只有 `// 待 M-5` 空 comment（Godot EnemyManager.cs:48 有 `e.StartRespawn()`）。
→ 修法：(1) FireDps 改 25、LavaDps 改 40、BoltDamage 改 12；(2) 死亡後補 `player.GainXp(E->GetXpReward())`；(3) Named/Boss 補 StartRespawn 呼叫；(4) Bolt 生成依賴 I-9 AI 整體修復。

⚠️ 部分不符（3項）：
- I-5 TakeDamage ActionBus 呼叫用 `DispatchPlayerDamage` 命名偏向玩家語意（C-6 修復後已接通，但命名可訂正為 `DispatchEntityDamage`）。
- I-7 StartRespawn 預設延遲 5f（AEnemy.h:116）vs Godot RespawnTime=8f（Enemy.cs:47），短了 3 秒。
- I-8 ApplyGravity：Godot 敵人墜出 world.Height 時 ForceDespawn（Enemy.cs:395），UE5 只做 InBounds 跳過不 ForceDespawn（可能造成敵人在未生成 chunk 無限懸空）；重力精度差異（Godot 亞格累加 _vy/_fractY，UE5 每幀單格落下）屬架構差異可接受。

### Agent J — §9 Items + §10 GameFlow/Save
**核實 21 項：✅12 / ⚠️5 / ❌3**（JSON：`agent-J.json`）

#### §9 Items / Equipment / Inventory

✅ 正確對應（9項，僅列摘要）：EItemId 枚舉整體結構（含 Block/Tool/Equip/Ore/Fragment 五類）、FItemData 11 欄位完整對應（Id/DisplayName/IsPlaceable/PlaceAs/IsTool/ToolTier/MiningSpeedMult/MaxStack/EquipSlot/AtkMult/DefFlat/MpBonus）、EEquipmentSlotType 四值順序、ItemRegistry 工具和裝備數值完整一致（ToolBasicPick Tier=1/Speed=2.5f、ToolIronPick Tier=2/Speed=3.5f、EquipBasicSword AtkMult=1.3f、EquipLeatherArmor DefFlat=5f、EquipAmulet MpBonus=30f）、UInventoryComponent TryAdd 雙輪算法（填補同種→填入空格）、Consume/SwapSlots 邊界守衛、GetActiveToolTier/GetActiveMiningSpeedMult、HotbarSize=10 / TotalSize=30 常數。

❌ J-11｜**DroppedItemManager 缺 SpawnFragments 碎片批次生成**：Godot `DroppedItemManager.cs:31-46` 有 `SpawnFragments(center, mat, tileCount, reason)` 方法，爆炸/採掘結束後批次呼叫：`units=tileCount/1000f`，Mining=`units*100` 碎片（全回收），Explosion=`units*RandRange(20,80)` 碎片。UE5 `UDroppedItemManager` 全文無此方法，爆炸後礦石碎片（Fragment* 系列物品）永遠不會掉落。
→ 修法：補上 `SpawnFragments(FGridPos Center, EMaterialType Mat, int32 TileCount, EDestroyReason Reason)`，查 `FMaterialRegistry::Get(Mat).FragmentItem`，依 Reason 計算碎片數量後呼叫 `SpawnDrop()`。

❌ J-12｜**ADroppedItemActor 缺重力落下邏輯**：Godot `DroppedItem.cs:13-35` 有 `FallInterval=0.18f` 計時器，每 0.18 秒若下方格（Y+1）為 Air 則 Position 往下移一格。UE5 `ADroppedItemActor::Tick`（ADroppedItemActor.cpp:27-33）只做 Age 累加，掉落物懸浮在被採掘的位置，無法落到地面。
→ 修法：Tick 中加 `FallTimer` 倒數（0.18f），到期後詢問 `AVoxelWorldActor` 正下方 tile 是否 Air，若是則 `SetActorLocation(Loc - FVector(0,0,30))`（30cm = 1 tile）。

⚠️ J-3｜**礦石物品 IsPlaceable 語意不符**：Godot `ItemRegistry.cs:41-44` 的 OreCoal/OreCopperRaw/OreIronRaw/OreMagicCrystal 四個礦石物品 `IsPlaceable=true`，`PlaceAs` 分別指向 CoalOre/CopperOre/IronOre/MagicCrystalOre（可把礦石物品放回地圖重建礦脈）。UE5 `ItemRegistry.cpp:52-56` 用 `MakeMat()` 建立，`bIsPlaceable=false`。玩家目前無法把礦石物品放回地圖。
→ 修法：改用 `MakeBlock(EItemId::OreCoal, ..., EMaterialType::CoalOre)` 等，對應各礦脈材質。

⚠️ J-2｜**OreGoldRaw 無 Godot 依據**：UE5 `ItemId.h:32` 獨自新增 `OreGoldRaw`，Godot `ItemId.cs` 無此值。`ItemRegistry.cpp:55` 把它當 `MakeMat`（不可放置），但 `MaterialRegistry` 是否有對應 GoldOre 礦脈和 FragmentGold 碎片待確認；若無，OreGoldRaw 在遊戲中是永遠無法獲得的 dead item。

⚠️ J-13｜**TryPickupAll 缺裝備自動穿戴邏輯**：Godot `DroppedItemManager.cs:62-90` 拾取裝備時，若對應裝備欄空則自動呼叫 `player.Equipment.TryEquip()`。UE5 `TryPickupAll` 只返回 `TArray<FItemStack>` 陣列。需確認 `ASkillCreatorCharacter` 呼叫端是否有補實作自動穿戴（未核實，列為警告）。

#### §10 GameFlow / Save

✅ 正確對應（3項，僅列摘要）：`FlowSaveSystem` 存取介面語意完整（ListAll/Create/Save/Load/Delete 對應 Godot Load/Save/SaveCharacter/SaveWorld/Delete）；`CreateNewWorld` GUID 生成（UE5 用完整 32 字元 GUID，Godot 用截頭 8 字元，碰撞機率更低）；`DeleteWorld` 遞迴刪除目錄語意一致。

❌ J-19｜**GameFlowWidget 缺世界預先地形生成（PregenerateWorld）**：Godot `GameFlowUI.cs:451-502` 在「確認創建世界」時同步執行 `PregenerateWorld()`，調用 `MapGenerator3D.Generate()`、把 chunk 存磁碟、寫 PlayerSpawn 到 world.meta，之後進入世界走「讀磁碟」快速路徑（Minecraft 式）。UE5 `UGameFlowWidget::CreateWorld()`（UGameFlowWidget.cpp:560-566）只呼叫 `FFlowSaveSystem::CreateNewWorld()` 建空目錄，無地形生成；`FWorldSaveData.bIsFirstEnter` 永遠維持 true，每次進入世界都要從頭生成地形，對 256 tiles 高度地圖會有明顯延遲。
→ 修法：在 `CreateWorld()` 後呼叫地形預生成邏輯（參考 `MapGenerator3D` + `TileWorld3D::SaveAllLoadedChunks`），或在 `GameMode::StartGameplayWithWorld` 偵測 `bIsFirstEnter==true` 時執行生成並存盤再設 `bIsFirstEnter=false`。

⚠️ J-16｜**進入世界時缺 LastPlayed 更新**：Godot `GameFlowUI.cs:379` 在進入世界時執行 `w.LastPlayed = DateTime.Now.ToString("yyyy-MM-dd HH:mm")` 並立刻存檔。UE5 `UGameFlowWidget::EnterWorld()`（UGameFlowWidget.cpp:575-589）無任何 LastPlayed 更新，`FWorldSaveData.LastPlayed` 永遠是初始值（空字串 / epoch）。
→ 修法：在 `EnterWorld()` 的 `OnEnterGame(*Found)` 呼叫前，更新 `Meta.LastPlayed = FDateTime::Now()` 並呼叫 `FFlowSaveSystem::SaveAll()` 或 `Meta.SaveMeta()`。

⚠️ J-21｜**CharacterSaveData Stamina/MentalEnergy/Mood 存盤語意與 Godot 不同**：Godot `CharacterSaveData.cs` 沒有 Stamina/MentalEnergy/Mood 欄位（這些是 runtime-only 狀態，每次進入世界重置）。UE5 `CharacterSaveData.h:24-26` 把它們列為 UPROPERTY() 持久化欄位，跨 session 保存。屬刻意擴充，不算錯誤，但設計意圖與 Godot 有語意差異需記錄。

### Agent K — §11 UI/Main/Input
**核實 24 項：✅10 / ⚠️8 / ❌6**（JSON：`agent-K.json`）

✅ 正確對應（10項，僅列摘要）：InputBindings 全鍵位完整對應（兩層架構：Enhanced Input Character + legacy BindKey PlayerController）、U/I/O/P 10槽組合鍵最長優先邏輯、採掘進度公式（SpeedMult×dt×60）與工具等級門檻、採掘形狀 GetOffsets 與預設 Cube、放置 bHoldToPlace 節流+rising-edge+0.12s 冷卻、1~9/0 熱鍵欄+滾輪+Ctrl縮放、面板鍵 V/B/N/Z/X/C/Q/F1~F6 全部對應、WalkSpeed/JumpZ/Gravity 數值換算（WorldScale 封裝）、SaveSpell 5 項驗證完整對應（全部收集後一次回報）、死亡重生 DeathGuard 攔截邏輯。

❌ K-5｜**PlacedObjectRegistry 完美移除分流缺失（已知缺口未修）**：Godot `Main.cs:1203-1225` 採掘完成後先查 PlacedObjectRegistry，若目標屬已放置物件且 _perfectRemove=true 走「移除整個 Unit + 返還 1 物品」路線。UE5 `OnMine()` 直接走形狀採掘，程式碼中只有自注釋說明此為已知缺口。
→ 修法：AVoxelWorldActor 加 TUniquePtr<FPlacedObjectRegistry>；OnMine 成功後先查 TryGetUnit(Target)；有 Unit 且 bPerfectRemove=true → RemoveUnit + Inventory->TryAdd(1)，不進入形狀採掘段；ASkillCreatorHUD 補 bool bPerfectRemove = true（Godot 預設 true）。

❌ K-12｜**F1 開發者筆刷（材質繪製）完全未實作**：Godot `Main.cs:549-599` F1 開啟含 Sand/Water/Stone/Wood/Fire/Lava 材質按鈕 + 4 種筆刷大小的面板，可直接在 Voxel World 繪製材質（左鍵）或清除（右鍵）。UE5 `OnDebugPaint()` 只切換一個 boolean + GEngine 偵錯文字「材質繪製尚未實作，僅狀態切換」，連 UI 面板都沒有。
→ 修法：ASkillCreatorHUD 加 F1 筆刷面板（6 種材質選擇 + 筆刷大小 1/3/5/9）；OnMine 若 bDebugPaintEnabled 則 TW->SetTile(target, ActivePaintMaterial)；右鍵若 bDebugPaintEnabled 則 TW->SetTile(target, Air)。

❌ K-15｜**積木編輯器關閉時未儲存變更確認彈窗缺失**：Godot `AbilityEditorUI.cs:264-327` TryExitEditor：_isDirty 時彈 ConfirmationDialog（「儲存並離開/捨棄變更」），還有 MP 超限警告。UE5 ToggleBlockEditorOverlay 直接隱藏視窗，玩家按 E 關閉時所有未儲存積木變更直接丟失。
→ 修法：SBlockEditorWidget 加 bool bIsDirty，OnChanged 中設 true，HandleSaveClicked 成功後清；新增 TryClose() 彈確認視窗後再關；ToggleBlockEditorOverlay 改呼叫 TryClose()。

❌ K-16｜**積木編輯器技能組 5 組 dot 切換 UI 缺失**：Godot `AbilityEditorUI.cs:193-250` Header 右側有 5 個 dot 按鈕切換 SpellGroup，切換時把當前 _spells[] 緩衝寫回再從新組讀取。UE5 SwitchEditorGroup API 存在但 PlayerController 只建立 1 個 UBlockEdGraph，5 組架構和 UI 完全缺失。
→ 修法：ToggleBlockEditorOverlay 建立 5 個 UBlockEdGraph 陣列；SBlockEditorWidget 加 5 個 dot 按鈕，點擊時 SwitchEditorGroup + 切換對應 Graph；切換前把當前 Graph.ToBlockNodes() 寫回對應 SpellLoadout.Slots。

❌ K-17｜**積木編輯器左側 Palette 面板完全缺失（最嚴重可用性缺口）**：Godot `AbilityEditorUI.cs:419-836` 左側有技能因子/積木/刻印三分頁 Palette，可點擊或拖放加入積木到畫布。UE5 SBlockEditorWidget::Construct 只有標題列 + 名稱+儲存行 + SGraphEditor 畫布，完全沒有 Palette 面板，積木只能靠右鍵選單加入。
→ 修法：SBlockEditorWidget::Construct 改為 SHorizontalBox，左側 250px SScrollBox 作 Palette，依 _activeLeftTab 切換三個 VBox（技能因子/積木/刻印）；按鈕點擊 AddNode() 到 Graph；拖放用 MouseMotion 偵測觸發 BlockDrag。

❌ K-18｜**積木編輯器右側統計面板完全缺失**：Godot `AbilityEditorUI.cs:932-1050` 右側有 AP 成本+進度條、BaseMpCost SpinBox、MP 逐類型分解、技能摘要。UE5 Header 只有名稱輸入 + 儲存按鈕一行，設計師完全無從得知技能的 AP/MP 成本。
→ 修法：SBlockEditorWidget 加 175px 右側面板：AP STextBlock + SProgressBar；BaseMpCost SSpinBox；MP breakdown SVerticalBox；描述 SScrollBox+STextBlock；OnChanged 時 RefreshStatsPanel() 更新所有值。

⚠️ 部分不符（8項，較次要）：K-7 放置 OccupiedByEntity 格排除缺失（TileWorld3D 有 API 但無人填入玩家/敵人座標）；K-9 移動物理架構差異（tile-step 離散 vs CharacterMovement 連續，StepH 60cm 需確認 MaxStepHeight 設定）；K-11 Tab 鍵 X-Ray 功能未移植（Tab 改為 CycleCameraMode 4模式，X-Ray 功能完全缺失）；K-19 容器導覽棧（麵包屑/ContainerEffect 深層編輯）缺失；K-20 AutoSave 30 秒自動存檔缺失；K-21 F2 座標 overlay 欄位不完整（只顯示格座標+世界座標，Godot 另有視角模式/螢幕px/玩家格座標/方向/OrthoZoom）；K-22 採掘/放置 mouseOverHotbar 阻斷缺失（滑鼠在 HUD 上仍可觸發世界互動）；K-23 InputBindings 玩家自訂鍵位持久化缺失（GetDefaultMappingContext API 已預留但持久化未實作）。

**建議新增待辦項目（中優先）**：
- [ ] **K-15** SBlockEditorWidget 補 bIsDirty 追蹤 + TryClose() 確認彈窗
- [ ] **K-16** 積木編輯器補 5 組 dot 切換 UI + 多 Graph 管理
- [ ] **K-17** SBlockEditorWidget 補左側 Palette 三分頁面板（技能因子/積木/刻印）
- [ ] **K-18** SBlockEditorWidget 補右側統計面板（AP/MP breakdown/摘要）
- [ ] **K-19** SBlockEditorWidget 補容器導覽棧（NavStack + 麵包屑 + ContainerEffect 深層編輯）
- [ ] **K-5** PlacedObjectRegistry 完美移除分流（AVoxelWorldActor + ASkillCreatorHUD bPerfectRemove=true）
- [ ] **K-12** F1 筆刷面板（材質繪製 UI + SetTile 實作）
- [ ] **K-20** ASkillCreatorCharacter::Tick 補 AutoSaveTimer（30f 觸發存檔）
- [ ] **K-7** 放置前 OccupiedByEntity 檢查（Tick 中更新 TW 佔用格 + OnPlace 前加 !IsOccupied 條件）

---

## 修復待辦清單（A~F 完成後彙整，尚未動手修，待使用者指示）

> 排序依嚴重度。每項附 id 可回 JSON 查完整佐證。G~K 尚未跑，待續。

### 🔴 高優先（架構性缺失/明顯偏離設計意圖）— **全部 12 項已於 2026-06-20 修復 ✅**

- [x] **D-8** `FEngraveData::CalculateEffect()` 補回 Hyperbolic/Linear 雙公式分支 + 7個缺失欄位（DisplayName/IsGlobal/BaseCost/RequiredPlayerLevel/Element/IsRestriction/TotalAbilityPointCost）— 影響所有刻印強度曲線
- [x] **B-7** act_area_fan 形狀公式重寫（寬度 `spread=(d+1)/2`、僅 Stone 擋格、整排擋住才中斷）
- [x] **B-9** act_area_beam 補正交寬度 + 修正長度公式（`18+BaseR`）
- [x] **B-10** ExecuteTechnique 補 Multi 重複迴圈 + 修正各武技半徑公式 + 補回4種舊版相容分支
- [x] **B-12** ExecuteMorph 依 TotemId 分四支還原（speed/strengthen/flight/invisible）
- [x] **B-15** ExecuteDomain 依 TotemId 分三支還原（barrier/terrain/weather）
- [x] **C-6** ActionBus：AEnemy 補上專屬 FActionBus 實例 + TakeDamageAmount 接通攔截
- [x] **C-7** EventBus 改為檔案級全域共享 TSet（GEventBusSignals）+ GFrameCounter 每幀清空，修復跨技能廣播物理缺口
- [x] **A-3** `ExecutionLoop.h::MaxWhileLoopIterations` 改引用 `FSafetyGuard::MaxWhileIterations`（1000→500）
- [x] **A-4** WaitCondition 無代理時 fallback 改回 `bMet=true`，對齊 Godot 同步路徑語意
- [x] **D-11** SpellSaveSystem 補未知 ManaTypeKey 防禦清除（`ValidateManaTypeKeys`，反序列化後檢查+清空+警告）
- [x] **D-4** EContainerType 補回 SummonMinion/SummonTurret/SummonGuardian 三獨立值，Area 移至尾端

  **修復細節與已知取捨**（完整佐證見 commit diff + 本文件上方 Agent A/B/C/D 詳細發現）：
  - B-5/B-14（ExecuteSummon）順手一併修復：補回 summon_minion/turret/guardian 視覺占位效果（原為純空函式）
  - C-10/D-13（AbilityPointCalculator 雙重計算疑慮）順手一併修復：`CalculateTotalCost` 改用 `FEngraveData::TotalAbilityPointCost()` 取代原本直接加總 `E.Points`
  - **設計判斷**：Godot 的 `PlayerController.Facing.Y` 因 2D→3D 遷移後恆為 0（ExecuteArea/Technique/Displacement/Summon/Domain 共用此 facing 欄位），導致 Godot 原始碼這幾個函式的「扇形/光束寬度」實際上會退化成垂直方向、寬度為 0 的退化形狀，並非設計意圖。修復時改用 UE5 既有的 FX/FZ（水平面前進方向）+ PX/PZ（水平垂直分量）取代，維持「水平扇形/光束」設計意圖，而非逐字複刻這個未遷移的 2D 遺留 bug；ExecuteTechnique/Domain/Summon 的半徑/距離數值常數本身（未乘 Grain）則逐行對齊 Godot，因為 Godot 本身對這些函式也沒做 Grain 縮放
  - Build 已驗證 0 錯誤 0 警告（順手修復 `SpellSlotSync.h` 缺少 `#include "AbilityPointCalculator.h"` 的既有隱性 bug，新 include 順序使其曝光）

### 🟡 中優先（功能性但範圍較小，或仍是已知技術債的延伸）

> **D-13 確認（2026-06-20）**：`FSpellSlot::AbilityPointCost` 在 UE5 只存 Totem 基礎成本（不含刻印），`CalculateTotalCost` 在計算時另外顯式加總 `LocalEngravings`（見 `AbilityPointCalculator.h:82-98`），無雙重計算。不需修改。

- [x] **B-5 / B-14** ExecuteSummon 補視覺占位效果 — 已於 🔴高優先段順手修復（2026-06-20）
- [x] **B-2** TryCast 的 MP 扣除改為多槽均分（2026-06-20：CalculateSlotCostByType 原子扣除，Godot SpellCaster.cs:251-275）
- [x] **B-3** 投射物方向改用鏡頭前向 + 身體邊緣起點偏移（2026-06-20：PC->GetPlayerViewPoint+halfW+1 偏移，Godot SpellCaster.cs:56-70）
- [x] **B-4** ExecuteContactHit 命中/未命中都提交 Runner.Submit（2026-06-20：加 Code 參數+FixedOrigin，Godot SpellCaster.cs:134-154）
- [x] **B-8** act_area_distant 距離公式修正（2026-06-20：`16+BaseR*2`，爆炸 `BaseR+2`，Godot SpellCaster.cs:581-583）
- [x] **B-11** act_fire_projectile 命中補 AoE + modifier 套用（2026-06-20：TW->Explode+ApplyModsToNearbyEnemies，Godot SpellProjectile.cs:114-116）
- [x] **B-13** act_teleport 步數改 20 倒數搜尋（2026-06-20：MaxSteps=20 i=20→1，Godot SpellCaster.cs:853-857）
- [x] **B-17** ApplyModsToNearbyEnemies 改逐格碰撞（2026-06-20：per-cell TileWorld check，Godot SpellCaster.cs:695-715；Slow/Freeze/Stun 待 W-5）
- [ ] **D-7** EEngraveCategory/EEngraveTrigger 列舉數值對齊 Godot 順序 + 補回 OnExpire
- [x] **C-10 / D-13** 確認無雙重計算（見上方 D-13 確認說明）
- [x] **C-5** SpellRunner.PruneAfter 補 MP 退還（2026-06-20：FActiveEntry.MpCost+OnMpRefund callback，Godot SpellRunner.cs:111-122）
- [x] **F-8** IWorldInterface.h 曼哈頓→歐幾里德距離注解（2026-06-20，Godot IWorldInterface.cs:11）
- [x] **F-10** IWorldInterface 補 GetEntityProperty/SetEntityProperty/CreateEntity 三方法（2026-06-20，Godot IWorldInterface.cs:12-19）
- [x] **F-3** UCharacterStateComponent Mood 雙向回歸：保留 UE5 擴充（2026-06-20 審查，Godot 標記「簡化規則待完整設計」）
- [x] **J-11** UDroppedItemManager 補 SpawnFragments() 批次碎片生成（2026-06-20：SpawnFragments(Center,Mat,TileCount,Reason)，Mining=100碎片/unit，Explosion=Rand(20~80)/unit；Godot DroppedItemManager.cs:31-46）
- [x] **J-12** ADroppedItemActor::Tick 補重力落下邏輯（2026-06-20：FallTimer+=dt，0.18f 到期查下方 Air→Z-TileSize；CachedVoxelWorld 由 SpawnDrop 注入；Godot DroppedItem.cs:13-35）
- [x] **J-3** ItemRegistry.cpp：OreCoal/OreCopperRaw/OreIronRaw/OreMagicCrystal 改為 MakeBlock+bIsPlaceable=true+PlaceAs 對應礦脈材質（2026-06-20；Godot ItemRegistry.cs:41-44；OreGoldRaw 為 UE5 擴充保持 MakeMat）
- [ ] **J-19** CreateWorld 後呼叫地形預生成（對應 Godot GameFlowUI.cs:451-502 PregenerateWorld；或在 GameMode::StartGameplayWithWorld 偵測 bIsFirstEnter=true 時生成）
- [ ] **J-16** EnterWorld() 補 LastPlayed 時間戳更新（Meta.LastPlayed=FDateTime::Now() + SaveMeta；Godot GameFlowUI.cs:379）
- [x] **G-3a** UpdateLiquid 入口加 Lava 限速（2026-06-20：`if (Mat==Lava && Frame%3!=0) return;`；Godot TileWorld3D.cs:265）
- [x] **G-3b** Lava UpdateLiquid 末尾點火機率改 0.1f（2026-06-20；Godot TileWorld3D.cs:287；原 0.005f 差 20 倍）
- [x] **G-9+G-10** MapGenerator3D GetHeightAt：freq 改 0.001f、octave 改 7、公式改 `WorldH*0.30+v*(WorldH*0.08)`、Clamp 改 `(H*0.15,H*0.45)`（2026-06-20；Godot MapGenerator3D.cs:70-79/247-249）
- [x] **I-11** AEnemyManager：FireDps 5→25、LavaDps 15→40、BoltDamage 8→12；死亡後補 `Char->GainXp(E->GetXpReward())`（2026-06-20；Godot EnemyManager.cs:486/488/72/61）
- [x] **H-5** MaterialRegistry.cpp：Wood RequiredToolTier 1→0、MagicCrystalOre RequiredToolTier 3→2、Coal Hardness 3.f→1.5f（2026-06-20；Godot MaterialRegistry.cs:26-75）

### 🟢 低優先（純文件訂正，程式碼本身沒問題）— **全部 5 項已於 2026-06-20 訂正 ✅**

- [x] **B-20** `docs/issues.md` 第15行§2統計摘要改為保守描述（~93%，列出 B-2/3/4/8/11/13/17 待修項目）
- [x] **C-2/C-3** `docs/issues.md` Submit/comboDepth 列改為「已實作」（MaxComboDepth=5 對齊 Godot）；TriggerCombo 列改為「已實作」（callback 拆分架構，非缺失）
- [x] **E-17** `實作進度.md` 已無「TakeSnapshot 暫緩」字樣（前次 session 已移除，確認 skip）
- [x] **A-14** `docs/issues.md` §1 VM Core 架構差異摘要補充：Tsm 查表機制 → instruction 直接攜帶 TotemId
- [x] **A-22** `docs/issues.md` §1 VM Core 架構差異摘要補充：VecCross 回傳型別由純量升級為 FVector（3D 外積）

---

## 注意事項

- 每個 Agent 完成後立刻將發現寫回本文件對應 section
- 修復時同步更新 issues.md 和 實作進度.md
- 有疑慮的項目先記錄，不要隨便修改
- 若 UE5 和 Godot 合理架構差異（如 ECS vs OOP 差異），不算錯誤
