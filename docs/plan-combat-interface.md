# 戰鬥介面重構計畫（ICombatant + FCombatResolver）

## 問題陳述

「敵人」是**立場標籤**（`IsHostile()==true`），不是種類。
但目前戰鬥管線（`TakePhysicalDamage` / `TakeEnergyDamage` / `TakeElementalDamage`）
重複存在於三個類別，且 `AEnemyManager` 只追蹤獸族（`AEnemy`），
導致立場是 Hostile 的 NPC 或召喚物完全不在戰鬥系統內。

## 目標架構

```
ECreatureKind    ← 「它是什麼」（Beast / NPC / Player / Summon / LifelikeEntity）
ICombatant       ← 「它能不能參與戰鬥」（與種類無關的能力契約）
IsHostile()      ← 「它現在的立場」（runtime 狀態，ICombatant 的一個方法）

「敵人」= 查詢條件：任何 ICombatant 且 IsHostile()==true
```

所有傷害公式集中在 `FCombatResolver`（不再在各類別重複）。

---

## 實作步驟

### C-1　新增 `ICombatant` 純 C++ interface

**位置：** `Source/SkillCreatorCore/Public/ICombatant.h`（無 gameplay 依賴，放 Core）

```cpp
class ICombatant
{
public:
    virtual ~ICombatant() = default;

    // ── 狀態查詢 ─────────────────────────────────────────────────
    virtual FCharacterStats& GetStats()          = 0;
    virtual const FCharacterStats& GetStats() const = 0;
    virtual float            GetHp()     const   = 0;
    virtual float            GetMaxHp()  const   = 0;
    virtual bool             IsAlive()   const   = 0;
    virtual FGridPos         GetPosition() const = 0;

    // ── 分類與立場 ───────────────────────────────────────────────
    virtual ECreatureKind    GetCreatureKind() const = 0;
    virtual bool             IsHostile()       const = 0;   // 決定是否算「敵人」

    // ── FCombatResolver 呼叫點 ──────────────────────────────────
    // Resolver 算完最終傷害後呼叫此函式；各類別在此扣 HP 並觸發死亡事件
    virtual void ApplyFinalDamage(float FinalDmg) = 0;
};
```

**注意：**
- `ICreature`（SkillCreatorCore 既有）已有 `GetHp/GetMaxHp/IsAlive/GetPosition`；
  `ICombatant` 可能重複部分方法。實作時評估是否讓 `ICombatant : public ICreature`
  或兩者並列（ICreature 給世界查詢，ICombatant 給戰鬥系統）。
- 無 `UCLASS/UINTERFACE`，純 C++ 虛基類（per CLAUDE.md）。

---

### C-2　新增 `FCombatResolver` 共用傷害解算器

**位置：**
- `Source/SkillCreatorRuntime/Public/FCombatResolver.h`
- `Source/SkillCreatorRuntime/Private/FCombatResolver.cpp`

```cpp
struct FCombatResolver
{
    // 物理傷害：兩步防禦（PhysicalDefense + PhysicalDamageReduction）+ 暴擊/閃避
    // 對應原 FCharacterStats::ResolvePhysicalDmg + 三個類別的 TakePhysicalDamage
    static void TakePhysicalDamage(
        ICombatant& Target,
        float Dmg,
        const FCharacterStats* Atk = nullptr,   // null = 無命中/暴擊判定
        AActor* OriginActor = nullptr);          // S-4 彈反用

    // 能量傷害：四步防禦
    static void TakeEnergyDamage(
        ICombatant& Target,
        float Dmg,
        const FCharacterStats* Atk = nullptr);

    // 元素傷害：抗性比例減傷 + 可選能量防禦路徑 + 暴擊
    static void TakeElementalDamage(
        ICombatant& Target,
        float Dmg,
        ESkillElementType Elem,
        bool bEnergyDefenseApplies = false,
        const FCharacterStats* Atk = nullptr);
};
```

**關鍵邏輯遷移：**
- 從 `ASkillCreatorCharacter::TakePhysicalDamage` 提取公式
- 從 `ANPCCharacter::TakePhysicalDamage` 確認一致（有則合併，有差異則記錄）
- 從 `ABeastCharacter::TakePhysicalDamage` 同上
- `FCharacterStats::ResolvePhysicalDmg`（H-1 已有）直接沿用

---

### C-3　`ASkillCreatorCharacter` 實作 `ICombatant`

**檔案：** `ASkillCreatorCharacter.h/.cpp`

- `.h`：加 `: public ICombatant`，宣告所有純虛方法
- `.cpp`：
  - `GetStats()` / `GetHp()` / `GetMaxHp()` / `IsAlive()` / `GetPosition()` → 回傳既有欄位
  - `GetCreatureKind()` → `ECreatureKind::Player`（既有）
  - `IsHostile()` → `return false;`（玩家永遠不是敵人）
  - `ApplyFinalDamage(float F)` → 原本 `TakeDirectDamage` 邏輯（扣 HP、死亡觸發）
  - `TakePhysicalDamage / TakeEnergyDamage / TakeElementalDamage` → 改成一行
    `FCombatResolver::TakeXxxDamage(*this, ...)` 轉發

---

### C-4　`ANPCCharacter` 實作 `ICombatant`

同 C-3 模式。差異點：
- `IsHostile()` → `return Disposition == EDisposition::Hostile;`
- `ApplyFinalDamage` → 原本 `TakeDamageAmount` 的 HP 扣除部分
- 確認 `TakePhysicalDamage/Energy/Elemental` 公式是否與 Character 完全一致，
  如有差異（NPC 可能有特殊減免）記錄到 FCombatResolver 的參數裡

---

### C-5　`ABeastCharacter` 實作 `ICombatant`

同 C-3 模式。差異點：
- `IsHostile()` → `return Hostility == EHostility::Hostile;`
- `ApplyFinalDamage` → 原本 `TakeDamageAmount` 的 HP 扣除部分

---

### C-6　`AEnemyManager` 改為管 `ICombatant`

**關鍵設計決策：UE5 GC 不追蹤裸 `ICombatant*`**

推薦方案：
```cpp
// 內部儲存維持 GC-safe（仍用 AActor*）
UPROPERTY() TArray<TObjectPtr<AActor>> CombatantActors;

// 對外暴露 ICombatant* 介面
TArray<ICombatant*> GetHostileCombatants() const;   // Cast + filter IsHostile()
ICombatant* FindHostileCombatantAt(FGridPos Pos) const;  // 取代 FindEnemyAt
```

`Spawn()` 改回傳 `ICombatant*`（實際仍 SpawnActor<AEnemy> 或未來的其他類別）。

**待解問題：**
- `AEnemyManager` 是否還需要「只管 Beast」的語意？
  若未來 NPC 轉敵對，NPC 由 `ANPCSpawnController` 生成、不經 `AEnemyManager`，
  則 `GetHostileCombatants()` 需要掃兩個來源（AEnemyManager + 世界上所有 ANPCCharacter）。
  → 評估是否改成一個全域的 `UCombatantRegistrySubsystem`（所有 ICombatant 實例在
    BeginPlay 時自我登記，EndPlay 時登出）取代 AEnemyManager 的追蹤角色。

---

### C-7　`ABaseProjectile` 改用 `ICombatant` 查目標

**`ASpellProjectile::OnTileEntered` 的變更：**

```cpp
// 原本：FindEnemyAt(NextPos) → AEnemy*
// 改後：FindHostileAt(NextPos) → ICombatant*
ICombatant* Hit = FindHostileAt(NewTile);
if (Hit) { /* 呼叫 FCombatResolver 或直接 OnHitEnemy callback */ Destroy(); }
```

`FindHostileAt()` 從 `AEnemyManager::FindHostileCombatantAt()` 或
未來的 `UCombatantRegistrySubsystem` 查詢。

`PlayerTarget` 欄位也改型別：`TWeakObjectPtr<ASkillCreatorCharacter>` →
可考慮改成 `ICombatant*`（但 WeakPtr 問題需處理，可保留 AActor 的 WeakPtr
加 Cast 保護）。

---

### C-8　清除三個類別的重複傷害管線

C-3/C-4/C-5 完成後，三個類別各自的完整傷害函式體應只剩一行轉發。
確認無殘留重複邏輯後，將舊的函式體刪除。

---

## 可選後續：`UCombatantRegistrySubsystem`

若 C-6 評估後認為 AEnemyManager 的追蹤過於侷限：

```cpp
// UWorldSubsystem：所有 ICombatant 實例自我登記
class UCombatantRegistrySubsystem : public UWorldSubsystem
{
    void Register(AActor* Actor);       // BeginPlay 時呼叫
    void Unregister(AActor* Actor);     // EndPlay 時呼叫
    TArray<ICombatant*> GetHostile(FGridPos Near, float RadiusTiles) const;
    ICombatant* FindAt(FGridPos Pos) const;
};
```

`AEnemyManager` 降為純「生成控制器」（決定在哪生成 Beast），
戰鬥查詢全走 `UCombatantRegistrySubsystem`。

---

## 執行順序建議

```
C-1 → C-2 → C-3/C-4/C-5（可並行）→ C-6 → C-7 → C-8
```

C-1/C-2 是純新增（不改任何現有 .h，不需 Rebuild）。
C-3/C-4/C-5 每個都涉及 .h 繼承宣告 → 各自需要關 Editor + 完整 Rebuild。
建議三個一起改完再 Rebuild 一次（不要分三次）。

---

## 受影響的檔案清單

| 步驟 | 新增 | 修改 |
|------|------|------|
| C-1 | `SkillCreatorCore/Public/ICombatant.h` | — |
| C-2 | `SkillCreatorRuntime/Public/FCombatResolver.h` `SkillCreatorRuntime/Private/FCombatResolver.cpp` | — |
| C-3 | — | `ASkillCreatorCharacter.h/.cpp` |
| C-4 | — | `ANPCCharacter.h/.cpp` |
| C-5 | — | `ABeastCharacter.h/.cpp` |
| C-6 | — | `AEnemyManager.h/.cpp` |
| C-7 | — | `ABaseProjectile.h` `ASpellProjectile.h/.cpp` |
| C-8 | — | `ASkillCreatorCharacter.cpp` `ANPCCharacter.cpp` `ABeastCharacter.cpp`（刪重複邏輯） |

---

## 完成標準

- [ ] `TakePhysicalDamage / TakeEnergyDamage / TakeElementalDamage` 的公式只存在於 `FCombatResolver.cpp` 一個地方
- [ ] `AEnemyManager::GetHostileCombatants()` 回傳的生物不受種類限制（Hostile NPC 也進列表）
- [ ] `ASpellProjectile::OnTileEntered` 不再引用 `AEnemy*` 型別
- [ ] Build 0 錯誤 0 警告
- [ ] `實作進度.md` 更新
