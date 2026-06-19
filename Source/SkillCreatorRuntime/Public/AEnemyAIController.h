#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "AEnemyAIController.generated.h"

// 敵人 AI 控制器。
// M-4：Possess() 時嘗試啟動 BT。
// M-5：UBTTask_MoveOnGrid 填入 Tile A*；UBTTask_AttackPlayer 接 SpellCaster。
// 2026-06-19：BehaviorTreeAsset 改於 constructor 用 ConstructorHelpers::FObjectFinder
// 自動載入 /Game/BT_Enemy（不依賴 Blueprint 子類別手動指定，符合 CLAUDE.md「AIController
// 的 BehaviorTree 指定」一律 C++ 處理的規則）；找不到資產時保留 null，OnPossess 安全跳過。
UCLASS()
class SKILLCREATORRUNTIME_API AEnemyAIController : public AAIController
{
    GENERATED_BODY()
public:
    AEnemyAIController();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    TObjectPtr<class UBehaviorTree> BehaviorTreeAsset;

protected:
    virtual void OnPossess(APawn* InPawn) override;
};
