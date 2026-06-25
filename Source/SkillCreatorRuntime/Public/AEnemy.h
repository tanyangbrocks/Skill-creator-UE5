#pragma once
#include "ABeastCharacter.h"
#include "AEnemy.generated.h"

// 向後相容層——BP_Enemy.uasset 繼承此類。
// 新程式碼請直接使用 ABeastCharacter；此處不添加任何成員。
UCLASS()
class SKILLCREATORRUNTIME_API AEnemy : public ABeastCharacter
{
    GENERATED_BODY()
};
