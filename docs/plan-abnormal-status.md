# 異常狀態系統實作計畫

> 來源：`origin text setting/more abnormal.txt`
> 前置架構：`SpecialStatusTypes.h` / `USpecialStatusComponent`（2026-06-26 建立）

---

## 一、前置說明

### 「先列出暫不實作」項目
封印、移動禁制、攻擊禁制、昏迷、沮喪、頹廢、憤怒、瘋狂、幻象、虛弱、老化、詛咒、被追蹤、重創、興奮

這些項目**只在遊戲資料（`EAbnormalStatusId` enum）中佔位**，不建立對應的 `FAbnormalStatusEffect` 子類。
Phase A-1 的 enum 定義時一次列齊全部狀態（含暫不實作的），確保 ID 不會在日後補實作時衝突或需要重新編號。

### 瀕死停止閾值
定義：`HP <= MaxHP * 0.05f`（5%），以常數 `NearDeathRatio = 0.05f` 存在狀態基底或常量頭。

### 所有數字均為變數
持續時間、最大層數、每層傷害量均以建構子參數傳入，不用 constexpr，方便技能/裝備在外部修改。

---

## 二、架構缺口與擴充（Phase A，一次完成）

### A-1　`SpecialStatusTypes.h`（`SkillCreatorCore`）

新增至 `FAbnormalStatusEffect`：

```cpp
// OnProcess 加 target（週期傷害呼叫 TakeElementalDamage 用）
virtual void OnProcess(float DeltaTime, IElementalTarget* Target) {}

// 已有：GetSpeedPenalty / GetImmobilizes / GetDamageTakenBonus
//       GetDefensePenalty / GetAttackBonus / GetDefenseBonus

// 新增：
virtual float GetAttackPenalty()          const { return 0.f; }  // 恐懼
virtual float GetAttackSpeedPenalty()     const { return 0.f; }  // 恐懼
virtual bool  GetCannotCastSkills()       const { return false; } // 結凍/石化/暈眩/麻痺/封技/極度恐懼
virtual bool  GetCannotUseMP()            const { return false; } // 能量封印
virtual float GetLuckPenalty()            const { return 0.f; }  // 噩運
virtual float GetMiningSpeedPenalty()     const { return 0.f; }  // 採掘疲勞

// 正面狀態標旗
virtual bool  GetHasSuperArmor()          const { return false; } // 霸體
virtual bool  GetIsEthereal()             const { return false; } // 虛化
virtual bool  GetIsInvincible()           const { return false; } // 無敵
virtual bool  GetHasBasicElemResistance() const { return false; } // 基礎元素抵抗
virtual bool  GetHasAdvancedElemResistance() const { return false; } // 進階元素抵抗
```

移除舊的無參數 `OnProcess(float)` 或保留為預設轉發。

### A-2　`USpecialStatusComponent`（`.h/.cpp`）

新增聚合輸出欄位：
```cpp
bool  bCannotCastSkills         = false;
bool  bCannotUseMP              = false;
float TotalAttackPenalty        = 0.f;
float TotalAttackSpeedPenalty   = 0.f;
float TotalLuckPenalty          = 0.f;
float TotalMiningSpeedPenalty   = 0.f;
bool  bIsInvincible             = false;
bool  bIsEthereal               = false;
bool  bHasSuperArmor            = false;
bool  bHasBasicElemResistance   = false;
bool  bHasAdvancedElemResistance= false;
```

`RecalcAggregates`：
- 計算以上所有欄位
- **霸體蓋邏輯**：若 `bHasSuperArmor == true`，強制 `bIsImmobilized = false`、`bCannotCastSkills = false`

`ProcessEffects`：改呼叫 `E->OnProcess(DeltaTime, Target)`，Target 由 `Cast<IElementalTarget>(GetOwner())` 取得。

---

## 三、具體狀態實作（Phase B）

新建 `Source/SkillCreatorRuntime/Public/AbnormalStatusEffects.h`（宣告全部）
新建 `Source/SkillCreatorRuntime/Private/AbnormalStatusEffects.cpp`（實作）

### B-1　週期元素傷害類（共用模式）

內部用 `float TickTimer = 0.f` 累積，每 `TickInterval`（0.5s）觸發一次 `Target->TakeElementalDamage`（或 TakeDirectDamage）。
瀕死停止：呼叫前先查 `Target->GetHp() <= Target->GetMaxHp() * NearDeathRatio`。

| 狀態 | 疊加 | 最多 | 持續 | 每層/每 0.5s | 元素 | 瀕死停止 |
|------|------|------|------|------------|------|---------|
| 燒傷 FBurnStatus | ✓ | 10 | 3s | 10 火 | Fire | ✓ |
| 凍傷 FFrostbiteStatus | ✓ | 5 | 3s | 5 冰 | Ice | ✓ |
| 中毒 FPoisonStatus | ✓ | 99999 | 3s | 8 毒 | Poison | ✓ |
| 麻痺 FParalysisStatus | ✓ | 10 | 1s | 7 雷 | Lightning | ✓ |
| 暗蝕 FDarkErosionStatus | ✓ | 10 | 3s | 20 暗 | Dark | **否** |
| 流血 FBleedStatus | ✓ | 10 | 3s | 8 真實 | — (TakeDirectDamage) | ✓ |

麻痺額外：`GetImmobilizes()=true`、`GetCannotCastSkills()=true`

### B-2　行動封鎖類

| 狀態 | 疊加 | 持續 | GetImmobilizes | GetCannotCastSkills | GetDefensePenalty |
|------|------|------|--------------|-------------------|-----------------|
| 結凍 FFrozenStatus | ✗ | 外部傳入 | true | true | 1.0（完全移除） |
| 石化 FPetrificationStatus | ✗ | 外部傳入 | true | true | 1.0 |
| 暈眩 FStunStatus | ✗ | 外部傳入 | true | true | 0 |
| 癱瘓 FParalysisLightStatus | ✗ | 外部傳入 | true | **false**（技能可用）| 0 |
| 極度恐懼 FExtremeFearStatus | ✗ | — | true | true | 0 |

**極度恐懼**：`OnProcess` 每幀查 Owner 的 `SpecialStatusComp->GetStackCount("Fear")` < 10 時自行 `RemainingDuration = 0`（觸發移除）。

### B-3　鏈式觸發狀態

**凍傷 → 結凍**
- `FFrostbiteStatus::OnProcess`：當 `StackCount == MaxStacks`（5）且 `FreezeImmunityTimer <= 0`，呼叫 `OwnerComp->ApplyStatus(make_unique<FFrozenStatus>(2.f))` 並重置 `FreezeImmunityTimer = 5.f`
- `FreezeImmunityTimer` 存在 `FFrostbiteStatus` 自身（static per owner or stored in component?）→ 存在 component 的獨立欄位，或在 FFrostbiteStatus 裡，由疊加策略複用（非疊加 = 刷新，所有層共享一個 timer）

**恐懼 → 極度恐懼**
- `FFearStatus::OnProcess`：當 `OwnerComp->GetStackCount("Fear") >= MaxStacks`（10）且尚未有極度恐懼，呼叫 `ApplyStatus(make_unique<FExtremeFearStatus>())`
- `FExtremeFearStatus::OnProcess`：恐懼層數 < 10 時自動消除（見 B-2）

### B-4　特殊行為類

**窒息 FSuffocationStatus**
- 非疊加；由 `UCharacterStateComponent::TickOxygen` 在 Oxygen 歸零時觸發 `ApplyStatus`，Oxygen > 0 時 `RemoveStatus`
- `OnProcess`：每 0.5s `TakeDirectDamage(MaxHP * 0.15f)`，**不停止瀕死，可致命**

**潮濕 FWetStatus**
- `OnApply`：`Target` 的 AuraComp `ApplyImmediate(Water, 5s)`；自身無其他效果

**即死 FInstantDeathStatus**
- `OnApply`：`Target->TakeDirectDamage(1e9f)`；`RemainingDuration` 設極小值（0.001f）立即清除

**能量封印 FEnergySeaStatus**
- `GetCannotUseMP() = true`

**封技 FSkillSealStatus**
- `GetCannotCastSkills() = true`（被動技能也失效，由技能系統統一查此 flag）

**恐懼 FFearStatus**
- 可疊 10 層，3s
- 每層：`GetAttackPenalty() = Stacks * 0.1f`、`GetAttackSpeedPenalty() = Stacks * 0.1f`、`GetSpeedPenalty() = Stacks * 0.1f`

**噩運 FBadLuckStatus**
- 可疊 10 層，3s
- `GetLuckPenalty() = Stacks * 5.f`

**採掘疲勞 FMiningFatigueStatus**
- 可疊 3 層，180s
- `GetMiningSpeedPenalty() = Stacks * 0.3f`

### B-5　正面狀態

| 狀態 | 疊加 | 持續 | 標旗 |
|------|------|------|------|
| 霸體 FSuperarmorStatus | ✗ | 外部傳入 | GetHasSuperArmor=true |
| 虛化 FEtherealStatus | ✗ | 外部傳入 | GetIsEthereal=true |
| 無敵 FInvincibleStatus | ✗ | 外部傳入 | GetIsInvincible=true |
| 基礎元素抵抗 FBasicElemResistStatus | ✗ | 外部傳入 | GetHasBasicElemResistance=true |
| 進階元素抵抗 FAdvancedElemResistStatus | ✗ | 外部傳入 | GetHasAdvancedElemResistance=true |

---

## 四、系統接入（Phase C）

### C-1　技能施放守衛
`ASkillCreatorCharacter::TryCastSlot` 和技能施放入口：
```cpp
if (SpecialStatusComp && SpecialStatusComp->bCannotCastSkills) return;
```

### C-2　MP 消耗守衛
MP 消耗呼叫點（`USpellCaster` 或 `ManaSlot::Consume`）：
```cpp
if (SpecialStatusComp && SpecialStatusComp->bCannotUseMP) return false;
```

### C-3　攻擊/互動守衛
`ASkillCreatorCharacter::StartChargingAttack`、`BeginCarry`、跳躍、翻滾等基礎動作入口：
```cpp
if (SpecialStatusComp && SpecialStatusComp->bIsImmobilized) return;
```

### C-4　傷害管線接入
`FCombatResolver` 各管線：
- `bIsInvincible` → `ApplyFinalDamage(0)` 直接跳過
- `bHasBasicElemResistance` → 元素傷害管線改走「能量防禦 → 能量減傷 → 元素抗性」路徑（基礎元素）
- `bHasAdvancedElemResistance` → 同上但對所有元素生效

### C-5　窒息接入
`UCharacterStateComponent::TickOxygen`（或等效方法）：
```cpp
if (Oxygen <= 0.f && !SpecialStatusComp->HasStatus("Suffocation"))
    SpecialStatusComp->ApplyStatus(make_unique<FSuffocationStatus>());
else if (Oxygen > 0.f)
    SpecialStatusComp->RemoveStatus("Suffocation");
```

### C-6　採掘速度接入
採掘/挖掘間隔計算：
```cpp
float Interval = BaseInterval / (1.f - SpecialStatusComp->TotalMiningSpeedPenalty);
```
（`TotalMiningSpeedPenalty` 由 3 層各 0.3 合計，上限 clamp 至 0.9 避免除以零或負值）

### C-7　AI 接入
`ABeastCharacter::GetMoveInterval`（已有 SpeedPenalty）、`GetAttackInterval`：
```cpp
// 攻擊禁制
if (SpecialStatusComp && SpecialStatusComp->bIsImmobilized) return FLT_MAX;
// 攻擊力懲罰（已在 FCombatResolver 透過 GetAttackBonus 套用，此處攻速另算）
float Interval = BaseAttackInterval * (1.f + SpecialStatusComp->TotalAttackSpeedPenalty);
```

---

## 五、完成後驗證清單

- [x] Phase A Build 0 錯誤 0 警告
- [x] Phase B Build 0 錯誤 0 警告
- [ ] Phase C Build 0 錯誤 0 警告（含 .h 變更，需 Rebuild）
- [ ] 燒傷：疊 10 層後每層各自倒計時 3s，瀕死時停止傷害輸出
- [ ] 凍傷：疊滿 5 層觸發結凍 2s，5s 內再疊滿不再觸發
- [ ] 結凍：完全無法移動/攻擊/施技，物理防禦歸零，霸體可覆蓋
- [ ] 恐懼疊滿觸發極度恐懼，恐懼層數降低時極度恐懼自動消除
- [ ] 無敵期間受到任何傷害均為 0
- [ ] 霸體覆蓋結凍/暈眩的行動封鎖

---

## 六、未決事項

- `FreezeImmunityTimer`（凍傷的 5s 結凍免疫計時）存放位置：建議存在 `USpecialStatusComponent` 的 `TMap<FName, float> StatusCooldowns` 通用冷卻表，由狀態的 `OnRemove/OnProcess` 讀寫
- `GetAttackBonus`（正面）已在架構中，但「攻擊力懲罰」（負面）目前走 `GetAttackPenalty()`，FCombatResolver 需要在呼叫 TakePhysicalDamage 時從**攻擊方**讀 `TotalAttackPenalty`（現在只讀防禦方的修飾）。Phase C 已在 `FCombatResolver.cpp` 留 TODO 說明：FCombatResolver 目前簽章只傳 `const FCharacterStats*`（非 ICombatant*），攻擊方懲罰待簽章擴充時一併套入

## 七、待辦（暫不實作）

- [ ] **虛化碰撞層切換**：`FEtherealStatus::OnApply` 把 CapsuleComponent 的 CollisionProfile 切換為穿透模式，`OnRemove` 還原。涉及 `UCapsuleComponent::SetCollisionProfileName` 或逐一設定 `ECollisionChannel`，待有實際施法場景時再接入。
