#pragma once
#include "CoreMinimal.h"
#include "ItemId.h"

class ASkillCreatorCharacter;

// 純 C++ 介面（不 UINTERFACE）：實體物品撿取系統。
// 目前由 APhysicalItemActor 實作；未來 ADebrisActor 也可實作（G-10）。
class SKILLCREATORRUNTIME_API IPhysicalPickable
{
public:
    virtual ~IPhysicalPickable() = default;
    virtual float   GetMass()            const = 0;  // 投擲公式用（kg 等比）
    virtual EItemId GetInventoryItemId() const = 0;  // EItemId::None = 無法存入物品欄
    virtual int32   GetInventoryCount()  const = 0;  // 存幾個
    virtual FText   GetPickupHintText()  const = 0;  // HUD 顯示提示文字
    virtual void    OnCarried(ASkillCreatorCharacter* Carrier) = 0;  // 進入攜帶狀態
    virtual void    OnReleased(FVector ThrowVelocityCms) = 0;        // 放下/投擲（0=放下）
    virtual bool    IsPickable() const { return true; }              // 預設可撿（攜帶中 override 成 false）
};
