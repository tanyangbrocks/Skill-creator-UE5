#include "AEnemyAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "UObject/ConstructorHelpers.h"

AEnemyAIController::AEnemyAIController()
{
    static ConstructorHelpers::FObjectFinder<UBehaviorTree> BTFinder(TEXT("/Game/BT_Enemy"));
    if (BTFinder.Succeeded())
        BehaviorTreeAsset = BTFinder.Object;
    else
        UE_LOG(LogTemp, Warning, TEXT("AEnemyAIController: /Game/BT_Enemy not found — enemies will not run any AI"));
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (BehaviorTreeAsset)
        RunBehaviorTree(BehaviorTreeAsset);
    else
        UE_LOG(LogTemp, Warning, TEXT("AEnemyAIController::OnPossess: BehaviorTreeAsset is null, %s will not act"),
            InPawn ? *InPawn->GetName() : TEXT("(null pawn)"));
}
