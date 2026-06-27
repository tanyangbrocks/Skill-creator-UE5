#include "SkillCreatorAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "CharacterStats.h"
#include "ICombatant.h"

USkillCreatorAttributeSet::USkillCreatorAttributeSet()
{
    InitHealth(100.f);
    InitMaxHealth(100.f);
    InitMana(100.f);
    InitMaxMana(100.f);
    InitAttackPower(10.f);
    InitPhysicalDefense(0.f);
    InitEnergyDefense(0.f);
    InitMoveSpeedMultiplier(1.f);
    InitAttackSpeedMultiplier(1.f);
    InitMiningSpeedMultiplier(1.f);
    InitLuckMultiplier(1.f);
    InitIncomingDamage(0.f);
}

void USkillCreatorAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        const float Dmg = GetIncomingDamage();
        SetIncomingDamage(0.f);
        if (Dmg > 0.f)
        {
            // GAS-5 完成後由此呼叫 ICombatant::ApplyFinalDamage()。
            // 目前 HP 由 FCombatResolver → FCharacterStats 管理，不在此處改動。
        }
    }

    // 乘數箝位：不允許負值
    if (Data.EvaluatedData.Attribute == GetMoveSpeedMultiplierAttribute())
        SetMoveSpeedMultiplier(FMath::Max(0.f, GetMoveSpeedMultiplier()));
    if (Data.EvaluatedData.Attribute == GetAttackSpeedMultiplierAttribute())
        SetAttackSpeedMultiplier(FMath::Max(0.f, GetAttackSpeedMultiplier()));
    if (Data.EvaluatedData.Attribute == GetMiningSpeedMultiplierAttribute())
        SetMiningSpeedMultiplier(FMath::Max(0.f, GetMiningSpeedMultiplier()));
}
