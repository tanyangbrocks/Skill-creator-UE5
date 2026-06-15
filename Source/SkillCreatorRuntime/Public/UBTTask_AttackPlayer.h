#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "UBTTask_AttackPlayer.generated.h"

// BT Task：對玩家執行攻擊。
// M-4：stub，直接回傳 Succeeded。
// M-5：近戰直接 TakeDamage；Ranged 透過 SpellCaster 發射投射物。
UCLASS()
class SKILLCREATORRUNTIME_API UBTTask_AttackPlayer : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_AttackPlayer();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
