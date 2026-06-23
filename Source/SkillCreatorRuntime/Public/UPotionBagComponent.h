#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UPotionBagComponent.generated.h"

// S-6 接口：藥水袋系統（stub）。完整系統完成後以子類覆寫虛擬方法。
UCLASS(ClassGroup=(SkillCreator), meta=(BlueprintSpawnableComponent))
class SKILLCREATORRUNTIME_API UPotionBagComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    // Q：服用所有藥水袋各一瓶（stub — S-6 實作後覆寫）
    virtual void UseAllBags();
    // Y：開/關藥水袋面板（stub — S-6 實作後覆寫）
    virtual void TogglePanel();
};
