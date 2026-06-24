# NPC 分類架構 + 遊蕩詩人 實作計畫

> 撰寫日期：2026-06-23
> 來源規格：[`origin text setting/base NPC system.txt`](../origin text setting/base NPC system.txt)
> 狀態：**✅ 已完成（2026-06-23，全 8 步，Build 0 錯誤 0 警告；詳見 `實作進度.md`「UE5 最新完成」表）**
> 性質：Godot 階段從未做 NPC 系統（[實作進度.md](../實作進度.md) Phase 3 明確標記「NPC 系統 ⬜（依賴世界觀設計，延後）」），這是全新系統，不適用「先讀 Godot 源碼」規則。但 UE5 這邊**已經有大量可重用的地基**，見下方第一節。

---

## 一、現有架構掃描結果（這次最大的發現：地基比想像中完整）

### 1-1 NPCBrain plugin（已完成 M-NPC-0~3，規格說「最重要的是都有 NPCBrain」指的就是這個）

| 里程碑 | 狀態 | 提供什麼 |
|--------|------|---------|
| M-NPC-0 推理基礎 | ✅ | 本地 llama-server，`UNPCBrainSubsystem::SendMessages()` |
| M-NPC-1 身分系統 | ✅ | `FNPCIdentity`（`Name/Species/Faction/Role/Traits[]/SpeechStyle/Backstory`）+ 存讀（`{Saved}/NPCBrain/Identities/{NPCId}.json`），**生成後鎖定，不重新生成** |
| M-NPC-2 記憶系統 | ✅ | `UNPCMemoryComponent`：短期 buffer（cap 20）+ 滿了自動 LLM 壓縮成長期摘要 |
| M-NPC-3 世界感知 | ✅（但有遺留缺口） | `UNPCPerceptionComponent::TickPerception()`，但依賴 `IWorldInterface`，**目前沒有任何地方實作這個介面並呼叫 `SetWorldInterface()`**——`plan-npc-brain.md` 自己也寫了「這是既有的架構缺口…建議在 M-NPC-5/6 時一併處理」，本計畫就是那個「之後」 |
| M-NPC-4 Prompt/解析 | ⏳ 未做 | LLM 回應要解析出 `dialogue/action/emotion/memory_note` |
| M-NPC-5 對話介面 | ⏳ 未做 | 玩家跟 NPC 打字對話的 UI |
| M-NPC-6 行動系統 | ⏳ 未做 | LLM 輸出的 `action` 真正觸發遊戲行為（Attack/Flee/Follow/Trade…） |
| M-NPC-7 批量生成 | ⏳ 未做 | 一次生成一批不重複的 NPC 身分 |

**結論**：NPCBrain 是「大腦」（身分/記憶/感知/未來的對話與決策），但**完全沒有「身體」**——專案裡目前沒有任何 `AActor` 把這顆大腦放進世界裡走動、被打、打人。這正是規格「遊蕩詩人」需要的東西，也是本計畫的核心。

### 1-2 可直接重用的既有 Actor 模式

| 既有系統 | 檔案 | 可重用的部分 |
|---------|------|-------------|
| `AEnemy` | `Source/SkillCreatorRuntime/Public/AEnemy.h` | **幾乎是模板等級的參考**：`FCharacterStats Stats` + `TakePhysicalDamage/TakeEnergyDamage`（B-3 公式管線）、`MeshComp`（StaticMeshComponent 當身體）、`ICreature`+`IElementalTarget`+`ISnapshottable` 三介面、`AIState` 狀態機、`ActionBus` 傷害攔截、Respawn 機制 |
| `AEnemyAIController` + `UBTTask_MoveOnGrid`/`UBTTask_AttackPlayer`/`UBTService_UpdateTarget` | `Source/SkillCreatorRuntime/Public/` | tile-grid 移動、攻擊判定、Blackboard 更新，遊蕩詩人的「到處走」「反擊」可以直接仿這套寫新的 BT Task，不用從零设計移動系統 |
| `AMobSpawnController` | `Source/SkillCreatorRuntime/Public/AMobSpawnController.h` | 「玩家附近環形範圍隨機找地板生成、太遠 despawn」整套邏輯，規格「可能出現在任何地方」直接套用同一套生成環設計 |
| `FRaceRegistry` | `Source/SkillCreatorCore/Public/RaceRegistry.h` | 164 種已分類好的種族資料（W-10 角色創建用），規格要求 NPC 有「隨機種族」——**不要讓 LLM 自由生成種族名稱**，改成從這份既有清單隨機抽，跟玩家角色創建系統用同一套種族設定，世界觀才一致 |
| `UCombatStateSubsystem` | `Source/SkillCreatorRuntime/Public/UCombatStateSubsystem.h` | 「進入戰鬥」狀態判定模式，遊蕩詩人的 Neutral→Hostile 轉換可以參考同款設計（不一定要共用同一個 subsystem，性質不同：那個是玩家戰鬥連擊計時，這裡是個體敵意狀態） |

### 1-3 確認缺少的東西

- **`FNPCIdentity` 沒有「外貌」欄位**——規格要求「名字、性格、外貌、種族、背景勢力、身份」六項，現有 struct 只有 `Name/Species/Faction/Role/Traits/SpeechStyle/Backstory`，缺 Appearance。
- **沒有 Faction 登錄表**——`FNPCIdentity.Faction` 是自由文字（LLM 生成什麼就是什麼），沒有像 `FRaceRegistry` 一樣的固定勢力清單。規格提到「背景勢力」「超界聯盟」這類具體名詞，但目前世界觀文件（`docs/plan-worldlore-integration.md`）有沒有定義正式勢力清單需要先確認（見第九節開放問題）。
- **沒有任何「NPC 分類」資料層**——規格的整套分類樹（特殊/區域/通用 → 7 大類 → 18 個子類）目前完全不存在於程式碼裡，需要從零設計（但只是資料表，不是邏輯）。

---

## 二、NPC 分類資料層設計

規格明確說「目前主要是幫 NPC 分類，建構好架構」「其它都只需要先在遊戲的架構中列出即可」——這一節是**純資料表**，不含行為邏輯，只負責「這個分類存在、屬於哪個大類、目前有沒有實作」。

```cpp
// Source/SkillCreatorCore/Public/NPCKind.h
UENUM(BlueprintType)
enum class ENPCMacroCategory : uint8
{
    Special   UMETA(DisplayName="特殊NPC"),   // 規格 §1：特殊設計+特殊功能，暫不實作任何具體個體
    Regional  UMETA(DisplayName="區域NPC"),   // 規格 §2：限定特定區域，暫不實作任何具體個體
    General   UMETA(DisplayName="通用NPC"),   // 規格 §3：種族/勢力/身份隨機，僅用「性質」分類
};

UENUM(BlueprintType)
enum class ENPCGeneralKind : uint8
{
    TravelingMerchant   UMETA(DisplayName="旅行商人"),
    Traveler            UMETA(DisplayName="旅行者"),
    Transcendent         UMETA(DisplayName="超界執行者"),
    Criminal            UMETA(DisplayName="罪犯"),
    Bard                UMETA(DisplayName="吟遊詩人"),
    Villager            UMETA(DisplayName="村民"),
    Refugee             UMETA(DisplayName="逃難者"),
};

USTRUCT(BlueprintType)
struct FNPCSubtypeDefinition
{
    GENERATED_BODY()
    UPROPERTY() FName             Id;            // e.g. "WanderingBard"
    UPROPERTY() ENPCGeneralKind   Kind = ENPCGeneralKind::Bard;
    UPROPERTY() FText             DisplayName;
    UPROPERTY() FText             Description;   // 規格原文的括號說明，原樣保留
    UPROPERTY() bool              bImplemented = false;  // 目前只有 WanderingBard = true
};

// 對應 FRaceRegistry/FItemRegistry 同一套「靜態登錄表」慣例
struct SKILLCREATORCORE_API FNPCKindRegistry
{
    static const TArray<FNPCSubtypeDefinition>& AllSubtypes();
    static const FNPCSubtypeDefinition* Find(FName SubtypeId);
};
```

登錄內容直接照規格逐條轉錄（18 個子類，`bImplemented` 全部 `false` 除了 `WanderingBard`）：

| Id | Kind | bImplemented |
|----|------|--------------|
| CartMerchant（馬車商人） | TravelingMerchant | false |
| WalkingMerchant（步行商人） | TravelingMerchant | false |
| TreasureHunterTraveler（尋寶旅行者） | Traveler | false |
| PersonSeekerTraveler（尋人旅行者） | Traveler | false |
| Adventurer（冒險者） | Transcendent | false |
| GhostHunter（獵鬼士） | Transcendent | false |
| Hunter（獵人） | Transcendent | false |
| Thief（盜賊） | Criminal | false |
| WantedKiller（通緝犯殺人魔） | Criminal | false |
| Trafficker（人口販子） | Criminal | false |
| **WanderingBard（游蕩詩人）** | Bard | **true** |
| GatheringVillager（採集村民） | Villager | false |
| HuntingVillager（狩獵村民） | Villager | false |
| RefugeeChild（逃難孩童） | Refugee | false |
| Unconscious（昏迷者） | Refugee | false |

「特殊NPC」「區域NPC」兩個大類目前規格沒有列出任何具體子類（規格原文只說「先列出這種，暫不實作」），對應到 `ENPCMacroCategory::Special`/`Regional` 兩個列舉值存在即可，暫不需要 `FNPCSubtypeDefinition` 條目（沒有具體名字可登錄）。

---

## 三、`FNPCIdentity` 擴充

```cpp
// 新增欄位（NPCIdentity.h）
UPROPERTY() FString Appearance;          // 外貌描述（LLM 生成，自由文字，跟 Backstory 同等級）
UPROPERTY() FName   RaceId;              // 改成存 FRaceRegistry 的 Id（不是自由文字）
UPROPERTY() FName   SubtypeId;           // 對應 FNPCKindRegistry 的 Id（e.g. "WanderingBard"）
```

**身分生成流程調整**（`UNPCIdentityGeneratorSubsystem::LoadOrGenerate`）：
1. 呼叫端先指定 `SubtypeId`（哪種 NPC）
2. `RaceId` 在生成前用 `FRaceRegistry::AllSystems()`／`RacesInSystem()` 隨機抽一個（不再讓 LLM 自由發明種族名稱，跟玩家角色創建系統共用同一份種族表，世界觀才不會分裂成兩套）
3. LLM prompt 只負責生成「自由文字」欄位：`Name/Traits/SpeechStyle/Backstory/Appearance`，並且把第 2 步抽到的 `RaceId` 對應的 `DisplayName`/`Description` 餵進 prompt 當作設定依據（讓生成的背景故事符合該種族的既定描述）
4. `Faction` 暫時維持自由文字（見第九節開放問題 Q1，世界觀勢力清單尚未確認是否存在正式列表）

---

## 四、`ANPCCharacter`（NPC 的「身體」）

新類別，**參考 `AEnemy` 的既有模式**（不是繼承它——語義上 NPC 不是敵人，獨立類別比較不會把「永遠敵對」的假設帶進來，沿用本專案「三行重複好過早抽象」的慣例，跟 `ASkillCreatorCharacter`/`AEnemy` 各自獨立實作 `TakePhysicalDamage` 同一個道理）：

```cpp
UENUM(BlueprintType)
enum class ENPCDisposition : uint8
{
    Neutral,   // 預設：不主動攻擊玩家，可被互動/對話
    Hostile,   // 被玩家攻擊後轉換；持續到 HostileCooldown 計時結束或死亡
};

UCLASS()
class SKILLCREATORRUNTIME_API ANPCCharacter
    : public APawn
    , public ICreature
    , public IElementalTarget
    , public ISnapshottable
{
    GENERATED_BODY()
public:
    FName            NPCId;        // 對應 FNPCIdentity.NPCId，存檔/讀檔 key
    FNPCIdentity     Identity;      // 載入後快取（LoadOrGenerate 完成時寫入）
    FCharacterStats  Stats;         // 沿用 AEnemy 同款，依抽到的 RaceId 套用基礎係數（B 系列力量公式同一套）

    ENPCDisposition  Disposition = ENPCDisposition::Neutral;
    float            HostileCooldown = 0.f;   // 脫離戰鬥後幾秒恢復 Neutral（暫定 15s，見開放問題 Q2）

    UPROPERTY() TObjectPtr<USkeletalMeshComponent>    MeshComp;          // 跟 AEnemy 不同：AEnemy 是 StaticMeshComponent（純色方塊佔位），
                                                                          // NPC 從第一天就要接真模型，直接用 SkeletalMeshComponent（見下方「資產管線」）
    UPROPERTY() TObjectPtr<UNPCMemoryComponent>       MemoryComp;        // NPCBrain plugin 既有元件
    UPROPERTY() TObjectPtr<UNPCPerceptionComponent>   PerceptionComp;    // NPCBrain plugin 既有元件

    void TakePhysicalDamage(float PhysAtk, const FCharacterStats* AttackerStats = nullptr);
    // ↑ 公式抄 AEnemy::TakePhysicalDamage；額外行為：傷害 > 0 → Disposition = Hostile，
    //   並記一筆 UNPCMemoryComponent::AddMemoryEvent(Observe, "被玩家攻擊", bPermanent=false)

    // ICreature / IElementalTarget / ISnapshottable：簽章對齊 AEnemy 對應實作
};
```

**`IWorldInterface` 接通**（解決 M-NPC-3 遺留缺口）：新增 `FWorldInterfaceAdapter : public IWorldInterface`（橋接 `AVoxelWorldActor` + `AEnemyManager`，`plan-npc-brain.md` 已經點名這個做法），`ANPCCharacter::BeginPlay()` 呼叫 `PerceptionComp->SetWorldInterface(Adapter)`，並在 `Tick()` 呼叫 `PerceptionComp->TickPerception()`（沿用「手動累加器」慣例，跟 SpellRunner 同款）。這個 Adapter 寫好後，**敵人/未來其它生物的感知能力也能一起受益**，不是 NPC 專屬的一次性接線。

### 資產管線：模型/動作/音效一律透過 Asset Registry 取得（2026-06-23 補充）

**✅ 使用者確認（2026-06-23）**：模型、圖案、音效、動作等任何素材都要能輕易替換，不寫死路徑。`docs/plan-asset-registry.md` §5-1 已經設計好對應的 `UMobMeshRegistry`（`FMobMeshEntry`：`Mesh/Material/AnimBP`，`FName` 為 key，`UDataTable`），但標記「生物模型路線未定，等 M-4 角色系統後設計」，**從未真正建立**——本計畫把它撿回來實際落地，順便讓 `AEnemy`（目前是純色方塊）未來也能直接受益，不是 NPC 專屬。

- `ANPCCharacter::MeshComp`（`USkeletalMeshComponent`）在 `BeginPlay()` 用 `Identity.RaceId`（或 `SubtypeId`，視覺差異更看種族還是看身分，待美術資源到位再決定，本計畫先設計成可由 key 切換的查表結構，不綁定其中一種）查 `UMobMeshRegistry::Get(Key)`，拿到 `FMobMeshEntry.Mesh/Material/AnimBP`，`SetSkeletalMesh()`/`SetAnimInstanceClass()`
- 找不到對應 entry（素材還沒準備好）時 fallback 到一個預設骨架網格（避免裸模型/不可見），跟現有 `AVoxelWorldActor` 的「Registry 找不到 fallback 到 VoxelMaterial」是同一個容錯模式
- 音效（吟遊詩人之後可能要有演奏/說話音效）走既有規劃的 `UEffectRegistry`（`FEffectEntry.HitSound` 已有先例，可以加一個 `FNPCAudioEntry` 同款結構，`FName` key）
- UI 圖示（NPC 對話框頭像、地圖標記）對應 `plan-asset-registry.md` §十一「UI 圖示 DataTable」（也是標記暫緩，本計畫不強制這次做完，但介面設計上要預留 `IconKey: FName` 欄位，等圖示 DataTable 真正建立時直接接上，不用回頭改 `FNPCIdentity`/`ANPCCharacter` 的結構）

**素材實際進來的流程（FBX 模型/動作）**：放新模型不需要手動在 Editor 裡一格一格拖——比照 AR-B 材質登記表那次的做法（一次性 Python script，`unreal.AssetImportTask`），把 `.fbx` 丟進指定資料夾後，跑腳本批次匯入成 `USkeletalMesh`/`UAnimSequence` 並寫入 `UMobMeshRegistry` 對應的 `FMobMeshEntry`，全程不需要手動拖拽。**唯一仍需要人工的環節**：動畫狀態機（`UAnimBlueprint` 的 AnimGraph，例如 Idle↔Walk↔Attack 怎麼混合切換）是視覺化 Graph，跟材質 Graph 屬於同一類「畫圖不是填資料」的工作，沒辦法用腳本合理產生，需要在 Editor 裡手動接幾條線——如果動作需求夠簡單（例如遊蕩詩人只需要走路 loop），可以直接 C++ `GetMesh()->PlayAnimation()` 完全繞過 AnimBP，省掉這個人工步驟。

⚠️ **附帶發現**：目前 `ASkillCreatorCharacter`（玩家本人）跟 `AEnemy` 都還沒有接任何骨骼模型/動畫（[實作進度.md](../實作進度.md)「本次暫緩：角色動畫」），這不是 NPC 系統的缺口，是整個專案共通的狀態。`UMobMeshRegistry` 一旦建立，玩家角色也可以用同一套機制接上模型，不必另外設計。

---

## 五、遊蕩詩人（WanderingBard）專屬設計

規格逐句拆解：

> 他可能出現在任何地方 → 第六節 `ANPCSpawnController`
> 做出任何行為 → AI 行為樹，本計畫先做最小可行版本：隨機遊蕩 + 待機動作（演奏/閒晃，純視覺，不影響邏輯）
> 如果被玩家攻擊，會嘗試反擊 → `Disposition` 轉換 + 反擊 BT 分支

### 行為樹設計

```
ANPCAIController（新類別，仿 AEnemyAIController）
└─ BT_WanderingBard
   ├─ Service: BTService_UpdateNPCTarget（每 tick 更新 Blackboard「附近是否有玩家」「Disposition」）
   └─ Selector
      ├─ [Disposition == Hostile] → Sequence（追擊 + 攻擊，重用 UBTTask_MoveOnGrid + UBTTask_AttackPlayer，
      │                                       傷害公式用 ANPCCharacter::Stats，跟敵人共用同一套 B-3 管線）
      └─ [預設] → BTTask_WanderRandomly（新 Task：每隔隨機 N 秒，在目前位置 ±WanderRadius tile 內挑一個
                                          可行走格子當目標，呼叫既有 UBTTask_MoveOnGrid 移動過去）
```

`BTTask_WanderRandomly` 是本計畫唯一需要新寫的移動邏輯，其餘重用既有 BT Task。

### 反擊後的恢復

`HostileCooldown` 倒數到 0 且玩家離開 `GetDetectRange()` → `Disposition` 回到 `Neutral`（避免詩人永遠記恨；長期記憶這次攻擊事件還是會留在 `UNPCMemoryComponent` 裡，下次對話時 NPC 仍然「記得」，這是規格沒明說但 NPCBrain 既有記憶系統會自然提供的效果）。

### 暫不接的部分（明確排除，避免本計畫範圍蔓延）

- **對話**（M-NPC-5 還沒做）：遊蕩詩人此階段**不**強制要求能對話，先讓「存在於世界、走動、被打會反擊」這個最小迴圈成立。對話介面做完後再接上是獨立工作，不阻塞本計畫。
- **「做出任何行為」的具體行為清單**：規格本身沒有舉例，本計畫只做「遊蕩」這個最低限度行為，其它（唱歌特效、跳舞動畫等）留待美術/動畫資源到位後再擴充，不在本次資料/邏輯層規劃範圍。

---

## 六、`ANPCSpawnController`

新類別，**直接照抄 `AMobSpawnController` 的結構**，不重新設計演算法（同一份「環形範圍生成/超距 despawn」邏輯對 NPC 一樣適用，差異只在生成表的內容）：

```cpp
USTRUCT(BlueprintType)
struct FNPCTableEntry
{
    FName SubtypeId;     // 對應 FNPCKindRegistry，目前只會填 "WanderingBard"
    float Weight = 1.f;
};

UCLASS()
class ANPCSpawnController : public AActor
{
    // MinSpawnDist/MaxSpawnDist/DespawnHardDist 等常數沿用 AMobSpawnController 的數值
    // （規格沒有要求遊蕩詩人有不同的生成密度，先共用同一套參數，之後要調再拆）
    TArray<FNPCTableEntry> NPCTable;
    // Spawn 時：FindSpawnPos（沿用 AMobSpawnController::TryFindSpawnPos）
    //         → SpawnActor<ANPCCharacter>
    //         → UNPCIdentityGeneratorSubsystem::LoadOrGenerate(NewNPCId, OnReady)
    //         → OnReady 內設 Identity/RaceId/Stats
};
```

⚠️ 跟敵人生成共用 `AMobSpawnController` 還是另開 `ANPCSpawnController`，見第九節開放問題 Q3（建議另開，理由：NPC 跟敵人的 despawn 規則可能之後會分岔——例如商人 NPC 之後可能要持久化、不能說消失就消失，敵人沒有這個顧慮）。

---

## 七、存讀檔

- `FNPCIdentity` 已有獨立存檔（`{Saved}/NPCBrain/Identities/{NPCId}.json`），身分本身不需要新工作。
- **世界內實例狀態**（位置/`Disposition`/`Stats`/是否存活）目前沒有任何持久化機制——`ANPCCharacter` 是 `AMobSpawnController` 風格的動態生成物，比照敵人的處理方式（敵人目前**也沒有**位置存檔，重進世界會重新生成，見 `AEnemyManager`），本計畫第一階段**沿用同一個「不持久化世界實例，只持久化身分」的決策**，不在這次新增額外存檔格式。若之後規格要求「商人 NPC 要記得自己在哪」，屆時再設計（跟 `PlacedObjectRegistry`/`placed-registry.json` 那層的需求性質類似，可以參考但不是本計畫範圍）。

---

## 八、實作順序建議

1. `NPCKind.h/.cpp`（`FNPCKindRegistry`，純資料表，零風險，先做）
2. `FNPCIdentity` 擴充（Appearance/RaceId/SubtypeId 欄位 + 生成流程改用 `FRaceRegistry` 抽選）
3. `FWorldInterfaceAdapter`（解開 M-NPC-3 遺留缺口，敵人系統也受益，建議盡早做完）
4. `UMobMeshRegistry`（撿回 `plan-asset-registry.md` §5-1 擱置的設計，落地成真正的 DataTable + Python 批次匯入腳本，見第四節「資產管線」）
5. `ANPCCharacter` 骨架（Stats/MeshComp 透過 `UMobMeshRegistry` 取得模型/介面實作，先不接 AI，能 SpawnActor 站著不動就算這步完成）
6. `ANPCAIController` + `BTTask_WanderRandomly`（最小可行的「到處走」）
7. `Disposition` 狀態機 + 反擊邏輯（`TakePhysicalDamage` 觸發轉換 + BT Selector 分支）
8. `ANPCSpawnController`（讓遊蕩詩人真正出現在世界裡）

每階段完成後依 CLAUDE.md 規則更新 [實作進度.md](../實作進度.md)；`.h` 新增 UCLASS/UENUM/UPROPERTY 一律關閉 Editor + 完整 Rebuild。

---

## 九、開放問題（2026-06-23 使用者已全部確認照建議執行）

1. **✅** 「背景勢力」先維持自由文字（風險最小），等世界觀勢力清單確定再升級成登錄表。
2. **✅** `HostileCooldown` 暫定 15 秒，占位數值，待平衡。
3. **✅** NPC 另開 `ANPCSpawnController`，不跟 `AMobSpawnController` 共用。
4. **✅** 遊蕩詩人「演奏/閒晃」動畫/音效本次不接，排除在範圍外（但模型/動作素材管線——見第四節「資產管線」——已經設計好查表結構，動畫真正做出來時不需要回頭改資料結構）。
5. **✅** 「特殊NPC」「區域NPC」只建立 `ENPCMacroCategory` 列舉值，不臆測具體內容。
