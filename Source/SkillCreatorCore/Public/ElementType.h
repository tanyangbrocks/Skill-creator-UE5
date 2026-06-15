#pragma once
#include "CoreMinimal.h"
#include "ElementType.generated.h"

// 蒼究元素體系（對應 Godot ElementType.cs）
// 注意：EElementType 已被 SlateCore 佔用，改用 ESkillElementType
UENUM(BlueprintType)
enum class ESkillElementType : uint8
{
    None    = 0,
    Metal   = 1,
    Wood    = 2,
    Water   = 3,
    Fire    = 4,
    Earth   = 5,
    Ice     = 6,
    Wind    = 7,
    Light   = 8,
    Dark    = 9,
    Thunder = 10,
    Poison  = 11,
};
