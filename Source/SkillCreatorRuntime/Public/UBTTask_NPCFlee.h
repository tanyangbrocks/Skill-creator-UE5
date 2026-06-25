#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "UBTTask_NPCFlee.generated.h"

// M-NPC-6: reads ANPCCharacter::bFleeRequested; steps one tile away from the nearest
// player each BT tick. Returns Failed when bFleeRequested is false so the BT Selector
// falls through to the Wander branch. Tile-step approach matches UBTTask_WanderRandomly.
UCLASS()
class SKILLCREATORRUNTIME_API UBTTask_NPCFlee : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_NPCFlee();
protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
