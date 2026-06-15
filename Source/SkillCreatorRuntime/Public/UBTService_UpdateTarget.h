#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "UBTService_UpdateTarget.generated.h"

// BT Service：每 TickInterval 把玩家 Pawn 寫入 Blackboard。
// PlayerKey 預設名稱 "TargetActor"，在 BT 資產（M-8）中指定。
UCLASS()
class SKILLCREATORRUNTIME_API UBTService_UpdateTarget : public UBTService
{
    GENERATED_BODY()
public:
    UBTService_UpdateTarget();

    UPROPERTY(EditAnywhere, Category="Blackboard")
    FBlackboardKeySelector PlayerKey;

protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
