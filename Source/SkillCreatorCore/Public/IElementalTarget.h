#pragma once
#include "CoreMinimal.h"

// 可被元素狀態效果作用的對象（敵人、玩家）。
// 純 C++ 介面；不用 UINTERFACE，Cast<> 以靜態型別或多型判斷。
// 玩家 EntityId 固定為 -1；敵人各自遞增。
class IElementalTarget
{
public:
    virtual ~IElementalTarget() = default;
    virtual int32 GetEntityId()              const = 0;
    virtual void  TakeDirectDamage(float Amount)  = 0;
};
