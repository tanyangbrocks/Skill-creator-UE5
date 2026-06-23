#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UMapComponent.generated.h"

// S-7 接口：地圖系統（stub）。完整系統完成後以子類覆寫虛擬方法。
UCLASS(ClassGroup=(SkillCreator), meta=(BlueprintSpawnableComponent))
class SKILLCREATORRUNTIME_API UMapComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    // M：開/關地圖（stub — S-7 實作後覆寫）
    virtual void ToggleMap();
};
