#pragma once
#include "CoreMinimal.h"
#include "GridPos.h"

// 玩家與敵人的共同生物介面（對應 Godot ICreature.cs）。
// 純 C++ 介面；不依賴任何 Gameplay / UObject 模組。
// AEnemy / ASkillCreatorCharacter 實作此介面。
class ICreature
{
public:
    virtual ~ICreature() = default;
    virtual int32    GetCreatureId() const = 0;
    virtual FGridPos GetPosition()   const = 0;
    virtual float    GetHp()         const = 0;
    virtual float    GetMaxHp()      const = 0;
    virtual bool     IsAlive()       const = 0;
};
