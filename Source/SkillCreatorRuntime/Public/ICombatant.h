#pragma once
#include "CoreMinimal.h"
#include "CharacterStats.h"
#include "CreatureTypes.h"
#include "ElementType.h"
#include "GridPos.h"
#include "IElementalTarget.h"

class UElementalAuraComponent;
class AActor;

// 「能參與戰鬥」的能力契約（純 C++ 介面，SkillCreatorRuntime）。
// 不繼承 ICreature 以避免跨 module 虛繼承；重複宣告同名方法（GetHp/IsAlive/GetPosition），
// 實作類別的單一 override 同時滿足 ICreature 與 ICombatant 兩個介面。
//
// 「敵人」= 查詢條件：任何 ICombatant 且 IsHostile()==true。
class ICombatant
{
public:
    virtual ~ICombatant() = default;

    // ── 生命狀態（與 ICreature 同名；同一 override 滿足兩介面）────
    virtual FGridPos GetPosition() const = 0;
    virtual float    GetHp()       const = 0;
    virtual float    GetMaxHp()    const = 0;
    virtual bool     IsAlive()     const = 0;

    // ── 數值查詢 ─────────────────────────────────────────────────
    virtual FCharacterStats&       GetStats()       = 0;
    virtual const FCharacterStats& GetStats() const = 0;
    virtual ECreatureKind          GetCreatureKind() const = 0;

    // ── 元素系統橋接（消除 USpellCaster lambda 裡的型別轉換）───────
    virtual UElementalAuraComponent* GetAuraComp() const = 0;

    // ── AActor 橋接（UPROPERTY-safe 儲存用；各類別 return this）────
    virtual AActor*           AsActor()           = 0;
    // ── IElementalTarget 橋接（ApplyImmediate 第三參數；各類別 return this）────
    virtual IElementalTarget* AsElementalTarget() = 0;

    // ── 立場（決定是否算「敵人」）────────────────────────────────
    virtual bool IsHostile() const = 0;

    // ── 佔位判定（Heavy 等大型生物覆寫為多格）──────────────────────
    virtual bool OccupiesTile(FGridPos Pos) const { return GetPosition() == Pos; }

    // ── 特殊狀態修飾（FCombatResolver 在計算前讀取）──────────────
    // DefensePenalty：縮減 PhysicalDefense，[0,1]；0 = 無懲罰
    // DamageTakenBonus：放大進入傷害（物理/能量/元素均適用），>=0
    // 預設回傳 0 = 無狀態，各類別若有 SpecialStatusComp 則覆寫
    virtual float GetStatusDefensePenalty()   const { return 0.f; }
    virtual float GetStatusDamageTakenBonus() const { return 0.f; }

    // ── FCombatResolver 呼叫點 ──────────────────────────────────
    // Resolver 算完最終傷害後呼叫此方法；各類別在此扣 HP、ActionBus、觸發死亡
    virtual void ApplyFinalDamage(float FinalDmg) = 0;

    // ── 傷害進入點（外部統一使用，不感知具體類別）───────────────
    // 玩家覆寫 TakePhysicalDamage 以加入 S-4 彈反與元素接觸後處理
    virtual void TakePhysicalDamage(float PhysAtk,
                                    const FCharacterStats* Atk      = nullptr,
                                    AActor*               Attacker  = nullptr) = 0;
    virtual void TakeEnergyDamage(float EnergyAtk,
                                  FName  ManaTypeKey,
                                  const FCharacterStats* Atk = nullptr) = 0;
    virtual void TakeElementalDamage(float ElemAtk,
                                     ESkillElementType Elem,
                                     bool bEnergyDefenseApplies     = false,
                                     const FCharacterStats* Atk     = nullptr) = 0;
};
