#include "AEnemyAIController.h"
#include "BehaviorTree/BehaviorTree.h"

void AEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    // BT asset 在 M-8 建立；此處 stub — BehaviorTreeAsset 設定後自動啟動
    if (BehaviorTreeAsset)
        RunBehaviorTree(BehaviorTreeAsset);
}
