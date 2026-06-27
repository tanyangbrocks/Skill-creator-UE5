#pragma once
#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "SkillCreatorAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

// GAS-1：計算用屬性集。
// ⚠️ GAS-0~4 期間 Health/Mana 不是 HP/MP 的權威來源——FCharacterStats 才是。
// 這裡的值只作為 GE Execution 的「讀取參考」，初始化一次後 GE 不直接修改 HP 遊戲狀態。
// HP/MP 實際變動一律走 FCombatResolver → ICombatant::ApplyFinalDamage()。
UCLASS()
class SKILLCREATORRUNTIME_API USkillCreatorAttributeSet : public UAttributeSet
{
    GENERATED_BODY()
public:
    USkillCreatorAttributeSet();

    // ── 生命值（僅 GE 計算參考，非 HP 權威來源）──
    UPROPERTY(BlueprintReadOnly, Category="Attributes") FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(USkillCreatorAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, Category="Attributes") FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(USkillCreatorAttributeSet, MaxHealth)

    // ── 魔法值（同上）──
    UPROPERTY(BlueprintReadOnly, Category="Attributes") FGameplayAttributeData Mana;
    ATTRIBUTE_ACCESSORS(USkillCreatorAttributeSet, Mana)

    UPROPERTY(BlueprintReadOnly, Category="Attributes") FGameplayAttributeData MaxMana;
    ATTRIBUTE_ACCESSORS(USkillCreatorAttributeSet, MaxMana)

    // ── 攻擊 / 防禦（GE Modifier 捕捉用）──
    UPROPERTY(BlueprintReadOnly, Category="Attributes") FGameplayAttributeData AttackPower;
    ATTRIBUTE_ACCESSORS(USkillCreatorAttributeSet, AttackPower)

    UPROPERTY(BlueprintReadOnly, Category="Attributes") FGameplayAttributeData PhysicalDefense;
    ATTRIBUTE_ACCESSORS(USkillCreatorAttributeSet, PhysicalDefense)

    UPROPERTY(BlueprintReadOnly, Category="Attributes") FGameplayAttributeData EnergyDefense;
    ATTRIBUTE_ACCESSORS(USkillCreatorAttributeSet, EnergyDefense)

    // ── 速度乘數（Buff/Debuff 透過 GE Modifier 改這些值）──
    UPROPERTY(BlueprintReadOnly, Category="Attributes") FGameplayAttributeData MoveSpeedMultiplier;
    ATTRIBUTE_ACCESSORS(USkillCreatorAttributeSet, MoveSpeedMultiplier)

    UPROPERTY(BlueprintReadOnly, Category="Attributes") FGameplayAttributeData AttackSpeedMultiplier;
    ATTRIBUTE_ACCESSORS(USkillCreatorAttributeSet, AttackSpeedMultiplier)

    UPROPERTY(BlueprintReadOnly, Category="Attributes") FGameplayAttributeData MiningSpeedMultiplier;
    ATTRIBUTE_ACCESSORS(USkillCreatorAttributeSet, MiningSpeedMultiplier)

    UPROPERTY(BlueprintReadOnly, Category="Attributes") FGameplayAttributeData LuckMultiplier;
    ATTRIBUTE_ACCESSORS(USkillCreatorAttributeSet, LuckMultiplier)

    // ── 中繼傷害（Instant GE Execution 寫入，PostGameplayEffectExecute 轉交 FCombatResolver）──
    UPROPERTY(BlueprintReadOnly, Category="Attributes") FGameplayAttributeData IncomingDamage;
    ATTRIBUTE_ACCESSORS(USkillCreatorAttributeSet, IncomingDamage)

    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
