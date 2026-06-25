#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "UBTTask_NPCFollow.generated.h"

// M-NPC-6: reads ANPCCharacter::FollowTarget; steps one tile toward it each BT tick.
// Returns Failed when FollowTarget is null so the BT Selector falls through to Wander.
// Tile-step approach matches UBTTask_WanderRandomly.
UCLASS()
class SKILLCREATORRUNTIME_API UBTTask_NPCFollow : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_NPCFollow();
protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
