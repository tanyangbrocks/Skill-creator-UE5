#pragma once
#include "CoreMinimal.h"
#include "EquipmentSlotType.generated.h"

UENUM(BlueprintType)
enum class EEquipmentSlotType : uint8
{
    None      UMETA(DisplayName="無"),
    Weapon    UMETA(DisplayName="武器"),
    Armor     UMETA(DisplayName="防具"),
    Accessory UMETA(DisplayName="飾品"),
};
