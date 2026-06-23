#include "ANPCAIController.h"
#include "BehaviorTree/BehaviorTree.h"

ANPCAIController::ANPCAIController()
{
    // BT_WanderingBard.uasset 尚未在 Editor 建立，Cook 時 ConstructorHelpers 找不到資產
    // 會觸發 CDO Error 使打包失敗；改為由使用者在 Editor 設定 BehaviorTreeAsset。
    // OnPossess() 已有 null 檢查，BT 未設定時 NPC 不行動（不崩潰）。
}

void ANPCAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (BehaviorTreeAsset)
        RunBehaviorTree(BehaviorTreeAsset);
    else
        UE_LOG(LogTemp, Warning, TEXT("ANPCAIController::OnPossess: BehaviorTreeAsset is null, %s will not act"),
            InPawn ? *InPawn->GetName() : TEXT("(null pawn)"));
}
