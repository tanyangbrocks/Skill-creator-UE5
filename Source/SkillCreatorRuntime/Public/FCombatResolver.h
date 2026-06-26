#pragma once
#include "CoreMinimal.h"
#include "CharacterStats.h"
#include "ElementType.h"

class ICombatant;

// 集中所有傷害公式（B-3 管線）的靜態解算器。
// 從 ASkillCreatorCharacter / ANPCCharacter / ABeastCharacter 三份重複邏輯提取而來。
// 所有方法：計算最終傷害 → 呼叫 Target.ApplyFinalDamage()；不直接操作 HP。
struct FCombatResolver
{
    // 物理傷害：FCharacterStats::ResolvePhysicalDmg（命中/閃避/2步防禦/暴擊）
    // 回傳 true=命中（ApplyFinalDamage 已呼叫），false=miss/dodge
    static bool TakePhysicalDamage(ICombatant& Target, float Dmg,
                                   const FCharacterStats* Atk = nullptr);

    // 能量傷害：命中/閃避 → 4步能量防禦 → 暴擊
    static void TakeEnergyDamage(ICombatant& Target, float Dmg,
                                 FName ManaTypeKey,
                                 const FCharacterStats* Atk = nullptr);

    // 元素傷害：命中/閃避 → 元素抗性 → 可選能量防禦路徑 → 暴擊
    static void TakeElementalDamage(ICombatant& Target, float Dmg,
                                    ESkillElementType Elem,
                                    bool bEnergyDefenseApplies = false,
                                    const FCharacterStats* Atk = nullptr);

private:
    // 命中/閃避判定；miss = false，hit = true
    static bool CheckHitDodge(const FCharacterStats& Def, const FCharacterStats* Atk);
    // 暴擊/超暴擊倍率（Atk==null 或 Base==0 直接回傳 Base）
    static float ApplyCrit(float Base, const FCharacterStats& Def, const FCharacterStats* Atk);
};
