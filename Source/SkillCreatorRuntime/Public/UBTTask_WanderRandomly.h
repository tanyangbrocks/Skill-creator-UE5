#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "UBTTask_WanderRandomly.generated.h"

// BT Task：每隔隨機 N 秒，在目前位置 ±WanderRadiusTiles 內挑一個可行走格子當目標，
// 一次只移動一步（同 UBTTask_MoveOnGrid 的「每次只走幾步避免卡單幀」設計）。
// docs/plan-base-npc-system.md §五：本計畫唯一需要新寫的移動邏輯。
// 自我防呆：Disposition==Hostile 時直接 Failed（讓 Selector 改走攻擊分支），
// 不依賴 BT 圖一定要有對應的 Decorator。
UCLASS()
class SKILLCREATORRUNTIME_API UBTTask_WanderRandomly : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_WanderRandomly();

    UPROPERTY(EditAnywhere, Category="Wander") int32 WanderRadiusTiles  = 16;
    UPROPERTY(EditAnywhere, Category="Wander") float MinRetryInterval  = 3.f;
    UPROPERTY(EditAnywhere, Category="Wander") float MaxRetryInterval = 8.f;

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
