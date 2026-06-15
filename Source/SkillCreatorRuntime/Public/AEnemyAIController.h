#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "AEnemyAIController.generated.h"

// 敵人 AI 控制器。
// M-4：Possess() 時嘗試啟動 BT（BT asset 在 M-8 建立後指定）。
// M-5：UBTTask_MoveOnGrid 填入 Tile A*；UBTTask_AttackPlayer 接 SpellCaster。
UCLASS()
class SKILLCREATORRUNTIME_API AEnemyAIController : public AAIController
{
    GENERATED_BODY()
public:
    // BT 資產在 M-8 建立後在 Blueprint 子類別中指定
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    TObjectPtr<class UBehaviorTree> BehaviorTreeAsset;

protected:
    virtual void OnPossess(APawn* InPawn) override;
};
