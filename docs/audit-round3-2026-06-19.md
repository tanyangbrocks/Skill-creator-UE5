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
| G | §6 Voxel World Core（TileCell, TileWorld3D CA 物理, Chunk3D, MapGenerator3D） | World/TileWorld*.cs / World/MapGenerator*.cs / World/Chunk3D.cs | ⏳ 進行中 | — |
| H | §7 Rendering/Materials/Sky（GreedyMesh, MaterialData/Registry, PlacedObjectRegistry, TerrainFeature, SkyController） | Rendering/*.cs / Materials/*.cs | ⏳ 進行中 | — |
| I | §8 Enemy AI（Enemy, EnemyManager, EnemyProjectile, MobSpawnController, CameraController） | AI/*.cs / Enemy.cs / EnemyManager.cs | ⏳ 進行中 | — |
| J | §9 Items/Equipment/Inventory + §10 GameFlow/Snapshot/Save | Items/*.cs / Snapshot/*.cs / FlowSaveSystem.cs | ⏳ 進行中 | — |
| K | §11 UI/Main/Input（Main.cs 採掘/放置/鍵位, InputBindings, PlayerController 移動物理, AbilityEditorUI 儲存/驗證） | Main.cs / InputBindings.cs / PlayerController.cs / AbilityEditorUI.cs | ⏳ 進行中 | — |

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
*(待填入)*

### Agent H — §7 Rendering/Materials/Sky
*(待填入)*

### Agent I — §8 Enemy AI
*(待填入)*

### Agent J — §9 Items + §10 GameFlow/Save
*(待填入)*

### Agent K — §11 UI/Main/Input
*(待填入)*

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

- [ ] **B-5 / B-14** ExecuteSummon 補視覺占位效果（目前是純空函式，比 Godot 更倒退）
- [ ] **B-2** TryCast 的 MP 扣除改為多槽均分（目前永遠扣在找到的第一槽）
- [ ] **B-3** 投射物方向改用滑鼠/鏡頭瞄準座標取代 ActorForwardVector，並做身體外緣起點偏移
- [ ] **B-4** ExecuteContactHit 命中/未命中都應呼叫完整技能效果鏈（Runner.Submit），不只是固定傷害
- [ ] **B-8** act_area_distant 距離公式修正（目前 Grain 平方級錯誤縮放，UE5 距離是 Godot 的 7.6倍）
- [ ] **B-11** act_fire_projectile 命中補 AoE 範圍效果 + modifier 套用（push/pull/slow/freeze/stun）
- [ ] **B-13** act_dash/teleport 步數對齊 Godot 固定值（目前 240 vs 20，差 12倍）
- [ ] **B-17** ApplyModsToNearbyEnemies 改回逐格碰撞檢測（避免穿牆）+ 接通 Slow/Freeze/Stun（待 W-5）
- [ ] **D-7** EEngraveCategory/EEngraveTrigger 列舉數值對齊 Godot 順序 + 補回 OnExpire
- [ ] **C-10 / D-13** 確認 `FSpellSlot::AbilityPointCost` 是否已含 LocalEngravings，若是則移除 `CalculateTotalCost` 的重複加總
- [ ] **C-5** SpellRunner.PruneAfter 補回 MP 退還機制（若 Anchor/Rollback 積木仍依賴此語意）
- [ ] **F-8** IWorldInterface.h 註解修正（曼哈頓→歐幾里德），避免未來實作誤用
- [ ] **F-10** IWorldInterface 補 GetEntityProperty/SetEntityProperty/CreateEntity 三方法簽名（NPCBrain 感知系統需要）
- [ ] **F-3** UCharacterStateComponent Mood 下降分支確認是否保留（UE5 自行擴充，未對齊 Godot 單向回升）

### 🟢 低優先（純文件訂正，程式碼本身沒問題）

- [ ] **B-20** `docs/issues.md` 第15行「§2 ~97%/act_*全部完成」改為保守描述，與197-200行統一
- [ ] **C-2/C-3** `docs/issues.md:205-206` 移除「MaxComboDepth 8 vs Godot 5」「TriggerCombo stub」過期錯誤記錄（程式碼實際已正確）
- [ ] **E-17** `實作進度.md` 第316行移除「TakeSnapshot/RestoreFromSnapshot 暫緩」字樣（已完整實作）
- [ ] **A-14** issues.md 補充說明 Totem 查表機制已替換為直接攜帶 SlotRef（架構變更非缺失）
- [ ] **A-22** issues.md 補充說明向量系統 2D→3D 升級導致 VecCross 回傳型別由純量變向量

---

## 注意事項

- 每個 Agent 完成後立刻將發現寫回本文件對應 section
- 修復時同步更新 issues.md 和 實作進度.md
- 有疑慮的項目先記錄，不要隨便修改
- 若 UE5 和 Godot 合理架構差異（如 ECS vs OOP 差異），不算錯誤
