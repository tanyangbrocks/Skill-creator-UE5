#include "UBTTask_AttackPlayer.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "AEnemy.h"
#include "ASkillCreatorCharacter.h"

UBTTask_AttackPlayer::UBTTask_AttackPlayer()
{
    NodeName = TEXT("Attack Player");
}

EBTNodeResult::Type UBTTask_AttackPlayer::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    if (!AIC) return EBTNodeResult::Failed;

    AEnemy* Enemy = Cast<AEnemy>(AIC->GetPawn());
    if (!Enemy || !Enemy->IsAlive()) return EBTNodeResult::Failed;

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    // 近戰攻擊：直接扣血；Ranged 類型透過 SpellCaster 發射投射物（M-5 stub：直接傷害）
    ASkillCreatorCharacter* Player = nullptr;
    if (UObject* Obj = BB->GetValueAsObject(PlayerKey.SelectedKeyName))
        Player = Cast<ASkillCreatorCharacter>(Obj);

    if (!Player || !Player->IsAlive()) return EBTNodeResult::Failed;

    int32 Range = Enemy->GetAttackRange();
    FGridPos EPos = Enemy->GetPosition();
    FGridPos PPos = Player->GetPosition();
    int32 Dist = EPos.ChebyshevDistance(PPos);  // 棋盤格距離：對角線算 1 格，與 Manhattan 不同

    if (Dist > Range) return EBTNodeResult::Failed;

    Player->TakeDirectDamage(Enemy->GetAttackDamage());
    return EBTNodeResult::Succeeded;
}
