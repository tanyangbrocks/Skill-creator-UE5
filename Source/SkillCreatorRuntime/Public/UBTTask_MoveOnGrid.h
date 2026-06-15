#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "UBTTask_MoveOnGrid.generated.h"

// BT Task：BFS 路徑計算後在 tile grid 上移動 StepsPerExecution 格。
// Blackboard key "TargetActor"（由 UBTService_UpdateTarget 寫入）。
UCLASS()
class SKILLCREATORRUNTIME_API UBTTask_MoveOnGrid : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_MoveOnGrid();

    // 每次呼叫只移動幾步（避免長路徑卡單幀）
    UPROPERTY(EditAnywhere, Category="MoveOnGrid")
    int32 StepsPerExecution = 1;

    // 對應 UBTService_UpdateTarget 寫入的 key（預設名稱 "TargetActor"）
    UPROPERTY(EditAnywhere, Category="Blackboard")
    FBlackboardKeySelector PlayerKey;

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
