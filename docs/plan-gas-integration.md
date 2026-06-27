# GAS 整合計畫（Gameplay Ability System）

> 目標：以最大彈性整合 GAS，支援無限擴充的 Buff/Debuff 類型，同時完整保留 SpellCompiler VM（核心玩法）。
> 原則：**新增一種 Buff/Debuff 只需建立一份 `.uasset` 資料資產，零 C++ 改動。**

---

## 一、設計哲學

| 保留 | 用 GAS 取代 / 強化 |
|------|-----------------|
| SpellCompiler VM（積木編輯器核心） | 狀態數值計算層（Buff/Debuff 加成） |
| ExecutionLoop opcode 架構 | AttibuteSet（HP / MP / ATK / DEF / SPD …） |
| `USpecialStatusComponent`（UI 橋接） | GameplayEffect 資料資產（Buff/Debuff 定義） |
| `FCombatResolver`（傷害管線） | GameplayTag 層次結構（取代 FName StatusId） |
| 現有動作積木（Move / Wait / Explode …） | AbilitySystemComponent（掛載所有戰鬥實體） |

**SpellCompiler VM** 是本遊戲最獨特的設計，不替換。  
**GAS** 負責：① stat 加成計算 ② Buff/Debuff 生命週期管理 ③ 標籤過濾與免疫查詢 ④ 視覺/音效觸發（GameplayCue）。

---

## 二、GameplayTag 層次設計

在 `Config/DefaultGameplayTags.ini` 定義，全部標籤在此集中管理：

```ini
[/Script/GameplayTags.GameplayTagsList]
; ── 負面狀態（Debuff）──
+GameplayTagList=(Tag="Status.Debuff.Burn",        DevComment="燒傷")
+GameplayTagList=(Tag="Status.Debuff.Suffocation",  DevComment="窒息")
+GameplayTagList=(Tag="Status.Debuff.Wet",          DevComment="潮濕")
+GameplayTagList=(Tag="Status.Debuff.Frost",        DevComment="凍傷")
+GameplayTagList=(Tag="Status.Debuff.Frozen",       DevComment="結凍")
+GameplayTagList=(Tag="Status.Debuff.Poison",       DevComment="中毒")
+GameplayTagList=(Tag="Status.Debuff.DarkErosion",  DevComment="暗蝕")
+GameplayTagList=(Tag="Status.Debuff.InstantDeath", DevComment="即死")
+GameplayTagList=(Tag="Status.Debuff.Paralysis",    DevComment="麻痺")
+GameplayTagList=(Tag="Status.Debuff.EnergySeal",   DevComment="能量封印")
+GameplayTagList=(Tag="Status.Debuff.SkillSeal",    DevComment="封技")
+GameplayTagList=(Tag="Status.Debuff.Bleed",        DevComment="流血")
+GameplayTagList=(Tag="Status.Debuff.Petrify",      DevComment="石化")
+GameplayTagList=(Tag="Status.Debuff.Stun",         DevComment="暈眩")
+GameplayTagList=(Tag="Status.Debuff.Cripple",      DevComment="癱瘓")
+GameplayTagList=(Tag="Status.Debuff.Fear",         DevComment="恐懼")
+GameplayTagList=(Tag="Status.Debuff.Terror",       DevComment="極度恐懼")
+GameplayTagList=(Tag="Status.Debuff.Misfortune",   DevComment="噩運")
+GameplayTagList=(Tag="Status.Debuff.MiningFatigue",DevComment="採掘疲勞")
; ── 正面狀態（Buff）──
+GameplayTagList=(Tag="Status.Buff.SuperArmor",     DevComment="霸體")
+GameplayTagList=(Tag="Status.Buff.Phase",          DevComment="虛化")
+GameplayTagList=(Tag="Status.Buff.Invincible",     DevComment="無敵")
+GameplayTagList=(Tag="Status.Buff.ElemResBasic",   DevComment="基礎元素抵抗")
+GameplayTagList=(Tag="Status.Buff.ElemResAdvanced",DevComment="進階元素抵抗")
; ── 免疫標籤 ──
+GameplayTagList=(Tag="Immunity.Status.Frozen",     DevComment="結凍免疫冷卻")
+GameplayTagList=(Tag="Immunity.Status.Fear",       DevComment="恐懼免疫")
; ── 特性標籤（用於 GameplayEffect 條件查詢）──
+GameplayTagList=(Tag="Trait.Creature.Beast",       DevComment="野獸類生物")
+GameplayTagList=(Tag="Trait.Creature.NPC",         DevComment="NPC")
+GameplayTagList=(Tag="Trait.Creature.Player",      DevComment="玩家")
+GameplayTagList=(Tag="Trait.Element.Fire",         DevComment="火屬性實體")
+GameplayTagList=(Tag="Trait.Element.Ice",          DevComment="冰屬性實體")
```

> **擴充規則**：新增狀態 = 加一行 `+GameplayTagList`，然後建立對應 `UGameplayEffect` 資料資產，零 C++ 改動。

---

## 三、AttributeSet 設計

### `USkillCreatorAttributeSet`

```cpp
// Source/SkillCreatorRuntime/Public/SkillCreatorAttributeSet.h
UCLASS()
class USkillCreatorAttributeSet : public UAttributeSet
{
    GENERATED_BODY()
public:
    // ── 生命值 ──
    UPROPERTY() FGameplayAttributeData Health;
    UPROPERTY() FGameplayAttributeData MaxHealth;
    // ── 魔法值 ──
    UPROPERTY() FGameplayAttributeData Mana;
    UPROPERTY() FGameplayAttributeData MaxMana;
    // ── 體力 ──
    UPROPERTY() FGameplayAttributeData Stamina;
    UPROPERTY() FGameplayAttributeData MaxStamina;
    // ── 攻擊/防禦 ──
    UPROPERTY() FGameplayAttributeData AttackPower;
    UPROPERTY() FGameplayAttributeData PhysicalDefense;
    UPROPERTY() FGameplayAttributeData EnergyDefense;
    // ── 移速/攻速 ──
    UPROPERTY() FGameplayAttributeData MoveSpeedMultiplier;   // 預設 1.0
    UPROPERTY() FGameplayAttributeData AttackSpeedMultiplier; // 預設 1.0
    UPROPERTY() FGameplayAttributeData MiningSpeedMultiplier; // 預設 1.0
    // ── 運氣/其他 ──
    UPROPERTY() FGameplayAttributeData LuckMultiplier;        // 預設 1.0
    // ── 中繼傷害屬性（Instant GE 用，不持久）──
    UPROPERTY() FGameplayAttributeData IncomingDamage;        // 由 GE Execution 寫入

    // PostGameplayEffectExecute：死亡、HP 箝位、MP 箝位
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
```

`ATTRIBUTE_ACCESSORS` 宏同步生成 getter/setter（`Get`/`Set`/`InitXxx`）。

---

## 四、分階實作

### GAS-0：模組掛載（無破壞性，不動現有 C++）

**目標**：確認 GAS 可編譯。

1. `SkillCreatorUE5.uproject` → `Plugins` 加入：
   ```json
   { "Name": "GameplayAbilities", "Enabled": true }
   ```
2. `Source/SkillCreatorRuntime/SkillCreatorRuntime.Build.cs` 的 `PublicDependencyModuleNames` 加入：
   ```
   "GameplayAbilities", "GameplayTags", "GameplayTasks"
   ```
3. Build → 確認 0 錯誤（此階段不改任何 gameplay 邏輯）

---

### GAS-1：AbilitySystemComponent 掛載

**目標**：所有戰鬥實體持有 ASC + AttributeSet。

**修改檔案：**

| 類別 | `.h` 新增 | `.cpp` 新增 |
|------|----------|-----------|
| `ASkillCreatorCharacter` | `UAbilitySystemComponent* AbilitySystemComp`、`USkillCreatorAttributeSet* Attrs`、`IAbilitySystemInterface` 繼承 | constructor 建立元件；`GetAbilitySystemComponent()` 返回 |
| `ABeastCharacter` | 同上 | 同上 |
| `ANPCCharacter` | 同上 | 同上 |

**重要**：三個類別 `.h` 修改 → 需關 Editor + 完整 Rebuild。

`GetAbilitySystemComponent()` 實作（IAbilitySystemInterface 要求）：
```cpp
UAbilitySystemComponent* ASkillCreatorCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComp;
}
```

`PostGameplayEffectExecute` 的 HP/MP 死亡/箝位邏輯接入現有 `ApplyFinalDamage`。

> ⚠️ **GAS-0~4 期間 HP/MP 權威來源聲明**：`FCharacterStats` 是 HP/MP 的唯一權威來源。
> `USkillCreatorAttributeSet` 的 `Health`/`Mana` 僅在 GE Execution 計算時作為「讀取參考值」初始化一次，
> 不作為持久儲存。實際 HP/MP 變動一律走 `FCombatResolver → ICombatant`，
> `PostGameplayEffectExecute` 裡觸發的傷害也必須透過 `FCombatResolver::ApplyFinalDamage()` 執行，
> 不直接修改 AttributeSet，避免兩份 HP 值分叉導致死亡判斷觸發兩次。

---

### GAS-2：FStatusDisplaySnapshot 新增 StatusTag 欄位

**目標**：讓 UI 未來可直接查詢 ASC tag hierarchy（例如「有任何 Status.Buff.*？」「有 Immunity.Status.Fear？」），同時保留現有 `FName StatusId`，不破壞任何現有呼叫點。

```cpp
// SpecialStatusTypes.h — 純加法，FName StatusId 不動
struct FStatusDisplaySnapshot
{
    FName         StatusId;   // 既有欄位：圖示路徑 STA_{StatusId}、快取 key，不改
    FGameplayTag  StatusTag;  // 新增：供 UI 做 GAS tag hierarchy 查詢
    FText         DisplayName;
    float         TimeRemaining;
    int32         StackCount;
    EStatusPolarity Polarity;
};
```

`SyncFromASC()` 同時填兩個欄位（StatusId 取 tag leaf name 轉 FName）。  
`UAbnormalStatusBarWidget` 繼續讀 `StatusId`，**零改動**。  
此步驟需 Rebuild（`.h` 變動），但不需要更新任何現有呼叫端。

---

### GAS-3：GameplayEffect 資料資產（Buff/Debuff 完全資料驅動）

**目標**：每種狀態對應一份 `UGameplayEffect` `.uasset`，新增狀態不需 C++。

**資料資產命名規則**：`/Game/Abilities/GE_<StatusTagLeaf>`  
例：`GE_Burn`、`GE_Frozen`、`GE_SuperArmor`

**燒傷（GE_Burn）設定示意：**
- Duration Policy: Has Duration（6 秒）
- Period: 1.0 秒執行 Execution Calculation（觸發週期傷害）
- Modifier：`MoveSpeedMultiplier` Multiply 0.85（減速）
- Stacking：Aggregate by Target，MaxStacks=5，Duration Refresh On Stack
- GrantedTags：`Status.Debuff.Burn`
- RemoveTagsOnApplication：`Status.Debuff.Frozen`（燒傷移除結凍）
- ImmunityTags：`Immunity.Status.Burn`

**結凍（GE_Frozen）設定示意：**
- Duration: 5 秒
- Modifier：`MoveSpeedMultiplier` Override 0.0（完全不能移動）
- GrantedTags：`Status.Debuff.Frozen`
- ApplicationRequirements：`UGAReq_HasFrost`（必須先有凍傷才能觸發）

**霸體（GE_SuperArmor）設定示意：**
- Duration: 0（Instant Grant Tag）/ 或 Infinite（視技能決定）
- GrantedTags：`Status.Buff.SuperArmor`
- RemoveGameplayEffectsWithTags：`Status.Debuff.Stun`、`Status.Debuff.Cripple`（霸體清除控制）

**⭐ 新增任何 Buff/Debuff 的標準流程（零 C++）**：
1. `Config/DefaultGameplayTags.ini` 加 `Status.Buff.NewEffect` 或 `Status.Debuff.NewEffect`
2. Editor → Content Browser → `/Game/Abilities/` → 右鍵 → Blueprint Class → `UGameplayEffect` → 命名 `GE_NewEffect`
3. 設定 Duration / Modifiers / Stacking / Immunity（GrantedTags 已改為 Loose Tags，見下方說明）
4. 在 `GE_EffectRegistry.uasset`（DataAsset）加一列映射 `Status.Debuff.NewEffect → GE_NewEffect`

> **⚠️ GrantedTags → Loose Tags 架構決策（2026-06-27）**：
> UE5.7 Python API 無法寫入 GE 資產的 GrantedTags（`InheritableGameplayEffectTags` 在 UE5.4+ 已 deprecated，新的 `UInheritedTagsGameplayEffectComponent` 沒有 Python binding）。
> 改採 **Loose Tags** 方案：`USpecialStatusComponent::ApplyStatus()` 在呼叫 `ASC->ApplyGameplayEffectToSelf()` 的同時呼叫 `ASC->AddLooseGameplayTag(tag)`；`RemoveStatus()` 對應呼叫 `ASC->RemoveLooseGameplayTag(tag)`。
> 效果等同：`ASC->HasMatchingGameplayTag("Status.Debuff.Burn")` 仍然有效，GAS-2 加入的 `FStatusDisplaySnapshot::StatusTag` UI 查詢功能不受影響。
> 好處：不依賴 GE 資產設定，狀態生命週期與現有 C++ 系統完全同步；缺點：標籤不隨 GE 自動失效（但 RemoveStatus 明確移除，不會洩漏）。

---

### GAS-4：VM 橋接（SpellCompiler + ExecutionLoop）

**目標**：積木編輯器的 `ApplyStatus` 積木走 GAS 管線施加 GameplayEffect。

**新增 OpCode（`OpCode.h`）**：
```cpp
APPLY_GAS_EFFECT,   // Payload: FGameplayTag StatusTag, float Level, ETargetRef Target
```

**`FInstruction` Payload**（`Instruction.h` / `FInstancedStruct`）：
```cpp
struct FGasEffectPayload
{
    FGameplayTag StatusTag;
    float Level;
    ETargetRef Target;  // Self / Target / AllEnemies / AllAllies
};
```

**`ExecutionLoop::Step()` 新增 case**：
```cpp
case EOpCode::APPLY_GAS_EFFECT:
{
    auto& P = Instr.Payload.Get<FGasEffectPayload>();
    UGameplayEffect* GE = UGasEffectRegistry::Get(P.StatusTag);
    if (!GE) break;
    ResolveTargets(P.Target, Ctx, [&](AActor* T)
    {
        if (auto* ASC = T->FindComponentByClass<UAbilitySystemComponent>())
            ASC->ApplyGameplayEffectToSelf(GE, P.Level, SourceASC->MakeEffectContext());
    });
    break;
}
```

**`SpellCompiler::EmitBlock()` 新增 case**：
```cpp
case EBlockType::ApplyStatus:
{
    FInstruction I;
    I.OpCode = EOpCode::APPLY_GAS_EFFECT;
    I.Payload.InitializeAs<FGasEffectPayload>({
        .StatusTag = FGameplayTag::RequestGameplayTag(Block.Params.GetRef<FName>("Tag")),
        .Level     = Block.Params.GetRef<float>("Level"),
        .Target    = Block.Params.GetRef<ETargetRef>("Target"),
    });
    Out.Add(I);
    break;
}
```

`.h` 的 `EOpCode` 新增值 → 需 Rebuild。

---

### GAS-5：橋接現有 `USpecialStatusComponent`（UI 不斷線）

**目標**：把 `USpecialStatusComponent` 的讀／寫兩端都接上 GAS，現有所有呼叫點零改動，UI 介面不變。

#### 寫路徑：ApplyStatus → ASC（現有 C++ 呼叫點不需改）

```cpp
// USpecialStatusComponent.cpp
void USpecialStatusComponent::ApplyStatus(TUniquePtr<FAbnormalStatusEffect> Effect)
{
    FGameplayTag Tag = AbnormalStatusId::ToGameplayTag(Effect->GetStatusId());
    TSubclassOf<UGameplayEffect> GEClass = UGasEffectRegistry::Get(Tag);
    if (GEClass && OwnerASC)
    {
        OwnerASC->ApplyGameplayEffectToSelf(
            GEClass->GetDefaultObject<UGameplayEffect>(), 1.f,
            OwnerASC->MakeEffectContext());
    }
    // Effect 物件在此丟棄，狀態資料改由 ASC 持有
}

void USpecialStatusComponent::RemoveStatus(FName StatusId)
{
    FGameplayTag Tag = AbnormalStatusId::NameToTag(StatusId);
    OwnerASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(Tag));
}
```

所有分散在 `ASkillCreatorCharacter` / `ABeastCharacter` / 元素反應中的
`SpecialStatusComp->ApplyStatus(...)` / `RemoveStatus(...)` 呼叫**完全不用改**。

#### 讀路徑：ASC → Snapshot（UI 不斷線）

```cpp
void USpecialStatusComponent::SyncFromASC()
{
    if (!OwnerASC) return;
    CachedSnapshots.Reset();
    // 遍歷 OwnerASC 的 ActiveGameplayEffects
    // 取 GrantedTags 對應到 FStatusDisplaySnapshot
    //   StatusId  = FName(*Tag.GetTagName().ToString().RightChop(Tag.GetTagName().ToString().Find(TEXT("."), ESearchDir::FromEnd) + 1))
    //   StatusTag = Tag
    //   TimeRemaining / StackCount 從 ActiveGEHandle 讀取
    //   Polarity 從 Tag 前綴 "Status.Buff" vs "Status.Debuff" 判斷
}
```

`ASkillCreatorHUD` 每幀仍呼叫 `GetStatusSnapshots()`，底層資料已來自 GAS，UI 層無感。

---

### GAS-6：彈性擴充工具層

這層讓**未來 Buff/Debuff 擴充完全不需改核心 C++**：

#### 6-A：`USkillCreatorMMC_ScaledDamage`（ModMagnitudeCalculation）
可重用的傷害縮放計算類，讓 GE 的傷害量根據施法者 `AttackPower` 動態決定：
```cpp
float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override
{
    float ATK = Spec.GetCapturedSourceAttributeMagnitude(AttackPowerDef, EvalParams, ATK);
    return ATK * ScaleFactor;
}
```

#### 6-B：純 C++ 套用前置條件（取代 GAS ApplicationRequirement）

> **⭐ 設計決策（2026-06-28）**：GAS 的 `UGameplayEffectApplicationRequirement` 必須以 Blueprint 子類設定，
> 且需在 Editor 手動掛入 GE 資產，無法用 Python 批次處理，違反「最小化 Editor 操作」原則。
> 改採 **C++ predicate table** 方案，全部條件邏輯集中在 `UGasEffectRegistry`，零 Blueprint 依賴。

在 `UGasEffectRegistry` 裡加入套用條件表：

```cpp
// UGasEffectRegistry.h
// 套用前置條件：nullptr 表示無條件（任何時候都可套用）
TMap<FName, TFunction<bool(UAbilitySystemComponent& TargetASC)>> ApplicationConditions;
```

在 `Initialize()` 裡以 lambda 定義各狀態的條件（只有有前置條件的才需要寫）：

```cpp
// 結凍需要先有凍傷
ApplicationConditions.Add(TEXT("Frozen"), [](UAbilitySystemComponent& ASC) {
    return ASC.HasMatchingGameplayTag(
        FGameplayTag::RequestGameplayTag(TEXT("Status.Debuff.Frost")));
});

// 極度恐懼需要先有恐懼
ApplicationConditions.Add(TEXT("Terror"), [](UAbilitySystemComponent& ASC) {
    return ASC.HasMatchingGameplayTag(
        FGameplayTag::RequestGameplayTag(TEXT("Status.Debuff.Fear")));
});

// 即死需要 HP < 10%（只有 HP 極低時才能觸發）
ApplicationConditions.Add(TEXT("InstantDeath"), [](UAbilitySystemComponent& ASC) {
    const USkillCreatorAttributeSet* Attrs = ASC.GetSet<USkillCreatorAttributeSet>();
    if (!Attrs || Attrs->GetMaxHealth() <= 0.f) return false;
    return (Attrs->GetHealth() / Attrs->GetMaxHealth()) < 0.1f;
});
```

`Find()` 套用前先過 predicate：

```cpp
TSubclassOf<UGameplayEffect> UGasEffectRegistry::Find(
    FName StatusTagLeaf, UAbilitySystemComponent* TargetASC) const
{
    // 前置條件檢查
    if (TargetASC)
    {
        if (const auto* Cond = ApplicationConditions.Find(StatusTagLeaf))
            if (!(*Cond)(*TargetASC)) return nullptr;
    }
    const TSubclassOf<UGameplayEffect>* Found = Registry.Find(StatusTagLeaf);
    return Found ? *Found : nullptr;
}
```

**新增一個帶條件的狀態 SOP（GAS-6 完成後）**：
1. `Config/DefaultGameplayTags.ini` 加標籤（1 行）
2. `/Game/Abilities/` 建 `GE_Xxx.uasset`（Editor 操作）
3. `UGasEffectRegistry::CanApply()` 加一個 `if` 分支（純 C++，Live Coding 可）
4. 無需改 GE 資產，無需 Blueprint 子類

**實作位置**：
- `Plugins/AbilitySystem/Source/AbilitySystem/Public/UGasEffectRegistry.h` → `CanApply()` 宣告
- `Plugins/AbilitySystem/Source/AbilitySystem/Private/UGasEffectRegistry.cpp` → `CanApply()` 實作
- `Source/SkillCreatorRuntime/Private/USpellCaster.cpp` → `ApplyGasEffectFn` 在 `Find()` 前呼叫 `CanApply()`

**已定義的前置條件**：
| 狀態 | 條件 |
|------|------|
| Frozen | 必須有 `Status.Debuff.Frost` loose tag（凍傷疊層） |
| Terror | 必須有 `Status.Debuff.Fear` loose tag（恐懼疊層） |

> **注意**：HP% 門檻型條件（如「HP<10% 才能即死」）需要 AttributeSet 存取，屬跨模組依賴（AbilitySystem 不依賴 SkillCreatorRuntime）。這類條件應在呼叫端（`USpellCaster`）或 `SkillCreatorRuntime` 層自行過濾，`UGasEffectRegistry::CanApply()` 只做純 tag 查詢。

#### 6-C：GameplayCue（視覺/音效解耦）
每個 GE 可設 GameplayCue 標籤（`GameplayCue.Status.Burn` 等），  
對應 `UGameplayCueNotify_Static` 播放粒子/音效，完全資料驅動，不需 C++。

---

## 五、實作順序與依賴

```
GAS-0（模組）
    └─> GAS-1（ASC + AttributeSet）    ← Rebuild 1 次（.h 變動）
            └─> GAS-2（GameplayTag）   ← Rebuild 1 次（.h 變動）
                    ├─> GAS-3（GE 資料資產）  ← 純 Editor 操作，不需 Rebuild
                    └─> GAS-4（VM 橋接）       ← Rebuild 1 次（OpCode.h 變動）
                            └─> GAS-5（UI 橋接）   ← .cpp only，可 Live Coding
                                    └─> GAS-6（擴充工具）← 按需新增
```

---

## 六、新增 Buff/Debuff 的完整 SOP（GAS 全部接通後）

1. `Config/DefaultGameplayTags.ini` 加標籤（1 行）
2. `/Game/Abilities/` 建 `GE_Xxx.uasset`（Editor 操作）
3. `GE_EffectRegistry.uasset` 加一列映射（Editor 操作）
4. （可選）積木編輯器 ApplyStatus 積木下拉選單加新 Tag（`SBlockEditorWidget.cpp`，純資料，Live Coding 可）

**零 C++ 改動，Build 不需要。**

---

## 七、技術注意事項

- **AttributeSet 必須在 ASC 初始化前 Subobject 建立**（`CreateDefaultSubobject` in constructor）
- **Gameplay Ability 不需要**：本計畫不用 `UGameplayAbility`（施法邏輯已在 VM），只用 AttributeSet + GameplayEffect
- **Prediction（客戶端預測）**：單機不需要，`ASC->SetIsReplicated(false)` 關閉複製
- **`FGameplayEffectContextHandle`**：Source 設為施法者 ASC，Target 設為目標 ASC，供 MMC 取 Source 屬性計算
- **`PostGameplayEffectExecute`**：HP 死亡判斷需 bridge 到現有 `ApplyFinalDamage`，避免繞過 `FCombatResolver`
- **相容性**：GAS-0/1/2 完成前，現有 `USpecialStatusComponent` 繼續照常工作；GAS-5 完成後才正式切換資料來源
