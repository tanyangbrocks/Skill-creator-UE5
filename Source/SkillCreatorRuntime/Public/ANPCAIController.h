#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "ANPCAIController.generated.h"

// NPC AI 控制器（docs/plan-base-npc-system.md §五），仿 AEnemyAIController 模式：
// BehaviorTreeAsset 用 ConstructorHelpers::FObjectFinder 自動載入 /Game/BT_WanderingBard，
// 找不到資產時保留 null，OnPossess 安全跳過（符合 CLAUDE.md「AIController 的 BehaviorTree
// 指定」一律 C++ 處理的規則——但 BT 圖本身內部的節點連線是 .uasset 視覺化內容，仍需要在
// Editor 裡用 BTTask_WanderRandomly/UBTService_UpdateNPCTarget/既有 UBTTask_AttackPlayer
// 等節點手動搭一次，這是 CLAUDE.md 明列的合法例外：建立 Behavior Tree .uasset 本體）。
UCLASS()
class SKILLCREATORRUNTIME_API ANPCAIController : public AAIController
{
    GENERATED_BODY()
public:
    ANPCAIController();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    TObjectPtr<class UBehaviorTree> BehaviorTreeAsset;

protected:
    virtual void OnPossess(APawn* InPawn) override;
};
