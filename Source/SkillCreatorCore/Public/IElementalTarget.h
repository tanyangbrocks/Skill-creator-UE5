#pragma once
#include "CoreMinimal.h"
#include "ElementType.h"

struct FCharacterStats;

// 可被元素狀態效果作用的對象（敵人、玩家）。
// 純 C++ 介面；不用 UINTERFACE，Cast<> 以靜態型別或多型判斷。
// 玩家 EntityId 固定為 -1；敵人各自遞增。
class IElementalTarget
{
public:
    virtual ~IElementalTarget() = default;
    virtual int32 GetEntityId()  const = 0;
    virtual float GetHp()        const = 0;
    virtual float GetMaxHp()     const = 0;
    virtual void  TakeDirectDamage(float Amount) = 0;
    // 與 ICombatant::TakeElementalDamage 相同簽名，實作類別一個 override 同時滿足兩介面
    virtual void  TakeElementalDamage(float ElemAtk, ESkillElementType Elem,
                                      bool bEnergyDefenseApplies = false,
                                      const FCharacterStats* Atk = nullptr) = 0;
    // 賦予元素 Aura（FWetStatus 等狀態透過此方法呼叫，character 覆寫，預設空實作）
    virtual void  ApplyElementalAuraImmediate(ESkillElementType Elem, float Duration) {}
};
