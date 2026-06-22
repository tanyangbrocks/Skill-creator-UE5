# B 系列：基本數值系統實作計畫

**依據**：`origin text setting/base value system.txt`（設計文件）  
**對照**：現有 `CharacterStats.h`、`UCharacterStateComponent.h`、`CharacterSaveData.h`、`ASkillCreatorCharacter.cpp`  
**最後更新**：2026-06-22

---

## 一、現況 vs 設計文件落差總表

### ✅ 已對齊（無需動作）

| 設計文件欄位 | 現有實作位置 |
|---|---|
| 生命上限 HP | `FCharacterStats::MaxHpBase` |
| HP 回復（0.5s）| `FCharacterStats::HpRegenRate`（**值已存在，計時間隔見 B-4**）|
| 體力值 Stamina | `UCharacterStateComponent::Stamina` |
| 精力值 | `UCharacterStateComponent::MentalEnergy` |
| MP 總量 | `FManaSlot::Max`（per type，W-6）|
| MP 回復（0.5s）| `FManaSlot::RegenRate`（**值已存在，計時間隔見 B-4**）|
| MP 功率（stub）| `FCharacterStats::MpEfficiency` |
| MP 掌控度（stub）| `FCharacterStats::MpControl` |
| 二連擊率 | `FCharacterStats::DoubleHitRate` |
| 三連擊率 | `FCharacterStats::TripleHitRate` |
| 抗連擊率 | `FCharacterStats::AntiCombo` |
| 暴擊率 / 暴擊傷 | `CritRate` / `CritDmgMult` |
| 超暴擊率 / 超暴擊傷 | `SuperCritRate` / `SuperCritDmgMult` |
| 抗暴率 | `FCharacterStats::AntiCrit` |
| 閃避率 / 命中率 | `DodgeRate` / `HitRate` |
| 移速 / 攻速 | `MoveSpeedMult` / `AtkSpeedMult` |
| 顏值 / 氣質（stub）| `Appearance` / `Temperament` |
| 信任度（stub）| `TrustLevel` |
| 美味度 / 香氣度 / 素材珍稀度（stub）| `Deliciousness` / `Fragrance` / `MaterialRarity` |
| 洞察度 / 隱密度（stub）| `Insight` / `Stealth` |
| 各屬性親和力/輸出/抗性 | `ElemAffinity/ElemOutputMult/ElemResistance`（TMap<ESkillElementType>）|
| 各項天賦能力點（7 維）| `TalentConstitution` … `TalentLuck` |
| 各項基礎能力點（7 維）| `ConstitutionPoints` … `LuckPoints` |
| MP/技能/等級/非戰/派遣 經驗加成（stub）| `SkillExpBonus` / `LevelExpBonus` / `NonCombatExpBonus` / `DispatchEfficiency` |
| 幸運值 / 血脈強度 | `Luck` / `BloodlineStrength` |
| 生存四值（溫度/飢餓/口渴/氧氣）| `UCharacterStateComponent`（**HUD 顯示見 B-5**）|
| 心情值 / 健康狀態 | `Mood` / `HealthStatus`（StateComponent）|

---

### ❌ 缺口（需要實作）

| 設計文件欄位 | 缺口說明 | 實作任務 |
|---|---|---|
| 物理防禦（物防）| 現有 `BaseDefense` 是通用防禦，語義不明 | B-1 |
| 能量防禦（能防）| 同上，需獨立欄位 | B-1 |
| 物理減傷 | 現有 `DamageReduction` 是通用的 | B-1 |
| 能量減傷 | 同上，需獨立欄位 | B-1 |
| 特定 MP 防禦 | 需 `TMap<FName, float>`（key = ManaTypeKey）| B-1 |
| 特定 MP 減傷 | 同上 | B-1 |
| 力量（Strength）| 只有 W-10 分配點，缺少「計算後的力量值」供技能/武器引用 | B-1 |
| 能量攻擊力（EnergyAtk）| 現有 `Power` 語義偏物理攻擊，缺能量攻擊基礎值 | B-1 |
| 抗暴傷 | 設計文件有，現有只有 `AntiCrit`（抗暴率）| B-1 |
| 抗超暴率 | 缺 | B-1 |
| 抗超暴傷 | 缺 | B-1 |
| 好感度（Affection）| 現有 `AffinityScore` 語義不同（設計文件 = 人際好感）| B-1 |
| 各類緣份點 | 設計文件指多種 NPC 關係，現無對應 | B-1 |
| 通緝等級 | 無對應欄位 | B-1 |
| 各法則親和力/輸出力/抗性（stub）| 法則系統（非元素），需 `TMap<FName>` | B-1 |
| 各能量契合度/輸出力/抗性（stub）| 特定 MP 類型的能力強化，需 `TMap<FName>` | B-1 |
| 技能能力點 / 屬性效果能力點 / 法則效果能力點（stub）| 能力點三分類缺後兩個 | B-1 |
| 角色年齡 / 外貌年齡（玩家自填）| 屬於存檔資料，`CharacterSaveData` 缺欄位 | B-2 |
| HP/MP regen 0.5s 計時間隔 | 現有 per-frame 乘以 DeltaTime（效果類似但行為不完全相同；設計文件說 0.5s 一跳，以便 buff 給「不同間隔回復」）| B-4 |
| 傷害公式（物理 / 能量 4 步管線）| `TakeDamage()` 只用 `BaseDefense` 一步，完全沒有能量/物理分流 | B-3 |
| HUD 生存條（溫度/飢餓/口渴/氧氣）| 值在 StateComponent 但主畫面沒有顯示 | B-5 |

---

## 二、分任務詳細計畫

---

### B-1：`CharacterStats.h` 補齊欄位

**性質**：修改 `.h`（USTRUCT/UPROPERTY）→ **必須關閉 Editor + 完整 Rebuild**  
**影響檔案**：`CharacterStats.h`、`ASkillCreatorCharacter.cpp`（TakeDamage 讀舊欄位處）

#### 1-a 拆分防禦/減傷

移除：
```cpp
// 移除（合併語義造成歧義）：
float BaseDefense     = 0.f;
float DamageReduction = 0.f;
```

新增：
```cpp
// ── 物理防禦 ─────────────────────────────────────────────────
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense|Physical")
float PhysicalDefense          = 0.f;   // 受物理攻擊時：攻擊力 - 物理防禦 = 傷害
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense|Physical")
float PhysicalDamageReduction  = 0.f;   // 物理傷害結果再扣此值

// ── 能量防禦 ─────────────────────────────────────────────────
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense|Energy")
float EnergyDefense            = 0.f;   // 受能量攻擊時通用防禦（次於特定MP防禦）
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense|Energy")
float EnergyDamageReduction    = 0.f;   // 能量傷害結果再扣此值（次於特定MP減傷）

// ── 特定 MP 防禦 / 減傷 ──────────────────────────────────────
// key = FManaSlot::ManaTypeKey（如 "gui_dao"、"xian_li"）
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense|MP")
TMap<FName, float> MpSpecificDefense;         // 特定MP防禦
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense|MP")
TMap<FName, float> MpSpecificDamageReduction; // 特定MP減傷

// 輔助查詢（找不到 key 回傳 0）
float GetMpDefense(FName Key)           const { const float* V = MpSpecificDefense.Find(Key);         return V ? *V : 0.f; }
float GetMpDamageReduction(FName Key)   const { const float* V = MpSpecificDamageReduction.Find(Key); return V ? *V : 0.f; }
```

#### 1-b 攻擊欄位

```cpp
// ── 基本攻擊 ─────────────────────────────────────────────────
// Strength：計算後的「力量值」，供技能公式引用（如「200% 力量」）
// 實際值由 W-10 能力點計算後寫入（見 ASkillCreatorCharacter::ApplyCharacterSaveData）
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
float Strength   = 0.f;   // 最終力量（衍生自 StrengthPoints + TalentStrength）

// EnergyAtk：能量攻擊基礎值（帶 MP 類型的攻擊使用）
// 具體值 = EnergyAtk * ElemOutputMult[element] 或 MpOutputMult[manaKey]
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
float EnergyAtk  = 0.f;

// Power 保留：物理攻擊力（等同原語義），改名為 PhysicalAtk 可選
// 暫不改名以避免大範圍 .cpp 搜尋替換，保留為 Power
```

> **設計注意**：力量（Strength）= 終值，由 `ApplyCharacterSaveData()` 根據 `StrengthPoints`、`TalentStrength`、種族加成計算後寫入。公式待 W-10 完整設計後定義，**B-1 只開欄位**。

#### 1-c 抗暴擊/超暴擊缺口

```cpp
// 補充（Anti 系統缺口）
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
float AntiCritDmgReduction   = 0.f;   // 承受暴擊時，抵銷攻擊方的暴擊傷倍率
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
float AntiSuperCritRate      = 0.f;   // 抵銷攻擊方的超暴擊率
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
float AntiSuperCritDmgReduction = 0.f; // 承受超暴擊時，抵銷超暴擊傷倍率
```

#### 1-d 社會/互動 stub 補齊

```cpp
// ── 社會系統（stub，無 NPC 系統時不生效）──────────────────────
// AffinityScore 保留（保持向後相容），新增以下：
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Social")
float Affection   = 0.f;                   // 好感度
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Social")
TMap<FName, float> BondPoints;             // 各類緣份點（key = 緣份類型名）

// ── 引敵（stub）─────────────────────────────────────────────────
// 現有 Deliciousness/Fragrance/MaterialRarity 已對齊；補通緝等級：
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Threat")
int32 WantedLevel = 0;                     // 通緝等級
```

#### 1-e 法則 / 能量 三項 stub（TMap<FName>）

```cpp
// ── 法則三項（stub）──────────────────────────────────────────
// key = 法則名稱（如 "law_fire"、"law_space"）
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Law")
TMap<FName, float> LawAffinity;    // 各法則親和力
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Law")
TMap<FName, float> LawOutputMult;  // 各法則輸出力
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Law")
TMap<FName, float> LawResistance;  // 各法則抗性

// ── 能量三項（stub）──────────────────────────────────────────
// key = ManaTypeKey（與 FManaSlot 對應）
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|MpCompatibility")
TMap<FName, float> MpAffinity;     // 各能量契合度
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|MpCompatibility")
TMap<FName, float> MpOutputMult;   // 各能量輸出力
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|MpCompatibility")
TMap<FName, float> MpResistance;   // 各能量抗性
```

#### 1-f 能力點 stub 補齊

```cpp
// ── 能力點（W-10 分類擴充，初始 0）──────────────────────────
// 現有 ConstitutionPoints…LuckPoints（7 維基礎能力點）保留
// 補充以下三類（stub，分配規則待設計）：
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
int32 SkillAbilityPoints    = 0;   // 技能能力點
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
int32 ElementAbilityPoints  = 0;   // 屬性效果能力點
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
int32 LawAbilityPoints      = 0;   // 法則效果能力點
```

#### 1-g 同步修改 `ASkillCreatorCharacter.cpp` 中讀取舊欄位的地方

`TakeDamage()` 目前：
```cpp
float Final = FMath::Max(0.f, Modified - Stats.BaseDefense * (1.f - AuraComp->DefensePenalty));
```
B-1 完成後暫改為：
```cpp
// 暫時：UE TakeDamage pipeline 未分物理/能量時，先套物理防禦
float Final = FMath::Max(0.f, Modified - Stats.PhysicalDefense * (1.f - AuraComp->DefensePenalty));
float FinalDR = FMath::Max(0.f, Final - Stats.PhysicalDamageReduction);
TakeDirectDamage(FinalDR);
```
完整管線在 B-3 實作後替換。

---

### B-2：`CharacterSaveData.h` 補齊欄位

**性質**：修改 `.h` → **必須關閉 Editor + 完整 Rebuild**（可與 B-1 同次 rebuild）  
**影響檔案**：`CharacterSaveData.h`

新增：
```cpp
// 角色創建時玩家自填（非計算值）
UPROPERTY() int32 CharacterAge  = 0;   // 角色設定年齡
UPROPERTY() int32 AppearanceAge = 0;   // 外貌年齡（可與角色年齡不同）
```

---

### B-3：實作傷害公式管線

**性質**：修改 `.cpp` → 可 Live Coding  
**影響檔案**：`ASkillCreatorCharacter.h`（新增 API）、`ASkillCreatorCharacter.cpp`（實作）

#### 傷害公式（依設計文件第 24-29 行）

**物理傷害（2 步）：**
```
step1 = max(0,  PhysAtk - PhysicalDefense)          // 防禦抵銷，最低 0
step2 = max(0,  step1   - PhysicalDamageReduction)  // 減傷，最低 0
→ final = step2
```

**能量傷害（4 步，帶 ManaTypeKey）：**
```
step1 = max(0, EnergyAtkAmount - GetMpDefense(Key))        // 特定MP防禦（優先）
step2 = max(0, step1           - EnergyDefense)            // 通用能量防禦
step3 = max(0, step2           - GetMpDamageReduction(Key))// 特定MP減傷（優先）
step4 = max(0, step3           - EnergyDamageReduction)    // 通用能量減傷
→ final = step4
```

**暴擊判定（攻擊方 vs 防禦方 stats）：**
```
effectiveCritRate = max(0, Attacker.CritRate - Defender.AntiCrit)
if FMath::FRand() < effectiveCritRate:
    effectiveCritMult = max(1.0, Attacker.CritDmgMult - Defender.AntiCritDmgReduction)
    damage *= effectiveCritMult
    // 超暴擊判定（在暴擊基礎上繼續擲骰）
    effectiveSuperCritRate = max(0, Attacker.SuperCritRate - Defender.AntiSuperCritRate)
    if FMath::FRand() < effectiveSuperCritRate:
        effectiveSuperCritMult = max(1.0, Attacker.SuperCritDmgMult - Defender.AntiSuperCritDmgReduction)
        damage *= effectiveSuperCritMult
```

**連擊判定：**
```
effectiveDoubleHit = max(0, Attacker.DoubleHitRate - Defender.AntiCombo)
effectiveTripleHit = max(0, Attacker.TripleHitRate - Defender.AntiCombo)
// 連擊本身也存在連擊率（遞迴直到不觸發）
```

**命中/閃避：**
```
// Attacker.HitRate > 100% 的部份抵銷 Defender.DodgeRate
excessHitRate = max(0, Attacker.HitRate - 1.0)
effectiveDodge = max(0, Defender.DodgeRate - excessHitRate)
if FMath::FRand() < effectiveDodge → 閃避，傷害 = 0（顯示 "Miss"）
// 若 Attacker.HitRate < 100%：固有 miss 機率 = (1 - HitRate)
if FMath::FRand() > Attacker.HitRate → 閃避
```

#### 新增 API

```cpp
// ASkillCreatorCharacter.h（新增 public 函式）

// 帶完整 stats 管線的物理傷害（攻擊者數值由施法/武器系統傳入）
void TakePhysicalDamage(float PhysAtk, const FCharacterStats* AttackerStats = nullptr);

// 帶完整 stats 管線的能量傷害
void TakeEnergyDamage(float EnergyAtk, FName ManaTypeKey, const FCharacterStats* AttackerStats = nullptr);

// TakeDirectDamage() 繼續保留：繞過防禦管線（生存傷害、環境傷害、Debug 用）
```

#### 敵人傷害接收

`AEnemy` 目前只有 `Hp/MaxHp`，沒有 `FCharacterStats`。B-3 階段**暫不替換敵人端**，維持現有 `TakeDamageAmount(float Amount)`（直接扣 HP）。待 M-5 怪物屬性系統設計後再補 `AEnemy::TakePhysicalDamage/TakeEnergyDamage`。

---

### B-4：HP/MP regen 改為 0.5s 計時器

**性質**：修改 `.cpp` → 可 Live Coding  
**影響檔案**：`ASkillCreatorCharacter.cpp`

#### 現況問題

- HP regen：`Tick()` 中 `CurrentHp += Stats.HpRegenRate * DeltaTime`（per-second 等效）
- MP regen：`FManaSlot::Tick(Delta)` per-frame，`Current += RegenRate * Delta`（同上）
- 設計文件明確指定「每 0.5 秒恢復一次」（而非每幀平滑），原因：未來 buff 可以附加「不同間隔的回復量」，需要精確定義「一跳」的時間

#### 實作方式

```cpp
// BeginPlay() 中啟動 timer
FTimerHandle RegenTimerHandle;
GetWorldTimerManager().SetTimer(RegenTimerHandle, this,
    &ASkillCreatorCharacter::TickRegen, 0.5f, true);

// TickRegen()（每 0.5 秒觸發）
void ASkillCreatorCharacter::TickRegen()
{
    if (!IsAlive()) return;
    // HP regen（每次回復 HpRegenRate × 0.5，相當於設計文件「每 0.5s 恢復 HpRegenRate * 0.5」）
    // 注意：設計文件的 HpRegenRate 定義為「每 0.5s 恢復量」，不是每秒
    // → 如 HpRegenRate = 2，則每 0.5s 回復 2（每秒實際 4）
    // ⚠️ 需要確認 Godot 原始碼 HpRegenRate 是「每 0.5s 量」還是「每秒量」再決定係數
    if (Stats.HpRegenRate > 0.f && CurrentHp < Stats.MaxHpBase)
        CurrentHp = FMath::Min(Stats.MaxHpBase, CurrentHp + Stats.HpRegenRate);

    // MP regen（每個 slot 各自）
    for (FManaSlot& Slot : ActiveManaSlots)
        Slot.Current = FMath::Min(Slot.Max, Slot.Current + Slot.RegenRate);
}
```

從 `Tick()` 移除原本的：
```cpp
// 移除 Tick() 中這兩行
if (Stats.HpRegenRate > 0.f && CurrentHp > 0.f)
    CurrentHp = FMath::Min(Stats.MaxHpBase, CurrentHp + Stats.HpRegenRate * DeltaTime);
// （FManaSlot::Tick() 的呼叫點也移除）
```

> **⚠️ B-4 前必讀 Godot**：確認 Godot 端 `HpRegenRate` / `MpRegenRate` 的定義是「每 0.5s 的量」還是「每秒的量」，避免數值乘以錯誤係數。讀取 `C:\skill-creator\` 中對應的 regen 計算邏輯。

---

### B-5：HUD 生存條顯示（溫度/飢餓/口渴/氧氣）

**性質**：修改 `.cpp` → 可 Live Coding  
**影響檔案**：`UPlayerHUDWidget.h`、`UPlayerHUDWidget.cpp`

#### 前置條件（必須先做）

讀取 Godot 原始碼，確認：
1. 生存條在 HUD 上的**位置**（右上角？左下？）
2. 顯示**形式**（進度條？圓形？數值文字？）
3. 四條的**順序**與**顏色**
4. 是否有**臨界閾值**時的顏色/閃爍變化
5. **氧氣條**是否只在水中/密閉空間才顯示（否則常駐）

> 讀取路徑：`C:\skill-creator\`，搜尋 Survival / Thirst / Hunger / Oxygen / Temperature 相關 UI 程式碼

#### 初步設計方向（以 Godot 確認為準）

```
UPlayerHUDWidget::BuildSurvivalBars()
  → 在 RootCanvas 右側加一個垂直欄（VBox）
  → 4 條 UProgressBar：溫度（橘）/ 飢餓（棕）/ 口渴（藍）/ 氧氣（青）
  → NativeTick 每幀讀 StateComp 更新 4 條百分比
  → 溫度條特殊：正常（36.5℃）= 滿，過熱/過冷各以不同顏色標記
  → 氧氣條：bIsOxygenDeprived=false 時隱藏
```

---

## 三、執行順序

```
B-2 → B-1 → （close editor + 完整 Rebuild）→ B-3 → B-4 → B-5
│         │                                    │       │      │
存檔欄位  Stats 欄位                          傷害    Regen  HUD
小改動    大改動（必須同次 Rebuild）            .cpp    .cpp   .cpp
```

- **B-2 + B-1 合併一次 Rebuild**：兩者都改 `.h`，一起做避免多次重啟 Editor
- **B-3 ~ B-5 都是純 `.cpp`**：可以 Live Coding，或個別重新編譯，無需關閉 Editor

---

## 四、各任務影響範圍摘要

| 任務 | 改 .h？ | 需關 Editor？ | 影響 .cpp | 預估複雜度 |
|------|---------|--------------|-----------|-----------|
| B-1 | ✅ CharacterStats.h | ✅ | ASkillCreatorCharacter.cpp（TakeDamage 一處）| 中（欄位多但機械性）|
| B-2 | ✅ CharacterSaveData.h | ✅ | 無（純增欄位）| 低 |
| B-3 | ✅ ASkillCreatorCharacter.h（新 API）| ✅（.h 改動）| ASkillCreatorCharacter.cpp（傷害計算重寫）| 高 |
| B-4 | ❌ | ❌（純 .cpp）| ASkillCreatorCharacter.cpp（Tick + BeginPlay）| 低 |
| B-5 | ❌ | ❌（純 .cpp）| UPlayerHUDWidget.cpp | 中 |

> B-3 因為要在 `.h` 新增函式宣告，**也需要關 Editor + Rebuild**。  
> 建議 B-1 + B-2 + B-3 的 `.h` 改動一起做完，只 Rebuild 一次。

---

## 五、尚未決定的設計問題（實作前需確認）

1. **力量（Strength）計算公式**：`StrengthPoints` × `TalentStrength` / 100 × 種族係數 = Strength？公式待 W-10 完整設計後確認
2. **HpRegenRate 單位**：設計文件說「每 0.5 秒恢復的 HP 量」→ regen rate 是每 0.5s 的量，還是 Godot 內部存的是每秒量、只是 0.5s 一跳？**→ B-4 前必讀 Godot**
3. **敵人是否也需要完整 stats 管線**：目前 AEnemy 只有 HP，若要對敵人施加帶物理/能量分流的傷害，需要 AEnemy 也有 `FEnemyStats` 或 `FCharacterStats`。M-5 前可先跳過，B-3 只實作玩家受傷管線
4. **健康值（健康狀態）是否需要改為 float**：現有 `EHealthCondition` 是列舉，設計文件的「健康值」語義較像連續值。確認後再決定是否重構
5. **通緝等級（WantedLevel）型別**：int32（0~5 等級制）還是 float（連續）？暫定 int32
