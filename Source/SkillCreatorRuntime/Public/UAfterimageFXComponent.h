#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UAfterimageFXComponent.generated.h"

// S-8 接口：殘影 FX 系統（stub）。完整系統完成後以子類覆寫虛擬方法。
UCLASS(ClassGroup=(SkillCreator), meta=(BlueprintSpawnableComponent))
class SKILLCREATORRUNTIME_API UAfterimageFXComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    // L 後撤：在起始位置生成半透明殘影（stub — S-8 實作後覆寫）
    virtual void SpawnAfterimage(FVector Origin, FVector Direction, float Duration);
    // 鬼影無敵幀（stub — S-8 ActionBus 接通後覆寫）
    virtual void ApplyInvincibility(float Duration);
};
