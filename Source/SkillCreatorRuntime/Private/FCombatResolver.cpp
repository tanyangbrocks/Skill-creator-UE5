#include "FCombatResolver.h"
#include "ICombatant.h"

// ── 私有輔助 ────────────────────────────────────────────────────────────────

bool FCombatResolver::CheckHitDodge(const FCharacterStats& Def, const FCharacterStats* Atk)
{
    if (!Atk) return true;  // 無攻擊者資訊 = 自動命中
    if (Atk->HitRate < 1.f && FMath::FRand() > Atk->HitRate) return false;
    const float ExcessHit = FMath::Max(0.f, Atk->HitRate - 1.f);
    const float EffDodge  = FMath::Max(0.f, Def.DodgeRate - ExcessHit);
    if (FMath::FRand() < EffDodge) return false;
    return true;
}

float FCombatResolver::ApplyCrit(float Base, const FCharacterStats& Def, const FCharacterStats* Atk)
{
    if (!Atk || Base <= 0.f) return Base;
    const float EffCritRate = FMath::Max(0.f, Atk->CritRate - Def.AntiCrit);
    if (FMath::FRand() < EffCritRate)
    {
        const float EffCritMult = FMath::Max(1.f, Atk->CritDmgMult - Def.AntiCritDmgReduction);
        Base *= EffCritMult;
        const float EffSuperRate = FMath::Max(0.f, Atk->SuperCritRate - Def.AntiSuperCritRate);
        if (FMath::FRand() < EffSuperRate)
        {
            const float EffSuperMult = FMath::Max(1.f, Atk->SuperCritDmgMult - Def.AntiSuperCritDmgReduction);
            Base *= EffSuperMult;
        }
    }
    return Base;
}

// ── 公開方法 ─────────────────────────────────────────────────────────────────

// 物理傷害管線對應 FCharacterStats::ResolvePhysicalDmg（H-1 已集中公式）
bool FCombatResolver::TakePhysicalDamage(ICombatant& Target, float Dmg, const FCharacterStats* Atk)
{
    const float Final = FCharacterStats::ResolvePhysicalDmg(Dmg, Target.GetStats(), Atk);
    if (Final < 0.f) return false;  // miss / dodge
    Target.ApplyFinalDamage(Final);
    return true;
}

// 能量傷害：命中/閃避 → 4步防禦 → 暴擊
// 對應 ABeastCharacter::TakeEnergyDamage + ASkillCreatorCharacter::TakeEnergyDamage（邏輯完全一致）
void FCombatResolver::TakeEnergyDamage(ICombatant& Target, float Dmg, FName ManaTypeKey, const FCharacterStats* Atk)
{
    const FCharacterStats& Def = Target.GetStats();
    if (!CheckHitDodge(Def, Atk)) return;

    float Step1 = FMath::Max(0.f, Dmg    - Def.GetMpDefense(ManaTypeKey));
    float Step2 = FMath::Max(0.f, Step1  - Def.EnergyDefense);
    float Step3 = FMath::Max(0.f, Step2  - Def.GetMpDamageReduction(ManaTypeKey));
    float Final = FMath::Max(0.f, Step3  - Def.EnergyDamageReduction);
    Final = ApplyCrit(Final, Def, Atk);
    Target.ApplyFinalDamage(Final);
}

// 元素傷害：命中/閃避 → 元素抗性比例減傷 → 可選能量防禦 → 暴擊
// 對應 ABeastCharacter/ANPCCharacter/ASkillCreatorCharacter::TakeElementalDamage（邏輯完全一致）
void FCombatResolver::TakeElementalDamage(ICombatant& Target, float Dmg, ESkillElementType Elem,
                                          bool bEnergyDefenseApplies, const FCharacterStats* Atk)
{
    const FCharacterStats& Def = Target.GetStats();
    if (!CheckHitDodge(Def, Atk)) return;

    const float Resistance = FMath::Clamp(Def.GetElemResistance(Elem), 0.f, 1.f);
    float Step1 = Dmg * (1.f - Resistance);

    float Final = Step1;
    if (bEnergyDefenseApplies)
    {
        Final = FMath::Max(0.f, Step1 - Def.EnergyDefense);
        Final = FMath::Max(0.f, Final - Def.EnergyDamageReduction);
    }
    Final = FMath::Max(0.f, Final);
    Final = ApplyCrit(Final, Def, Atk);
    Target.ApplyFinalDamage(Final);
}
