#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "UBTTask_NPCCounterAttack.generated.h"

// BT Task：Disposition==Hostile 時的反擊分支（docs/plan-base-npc-system.md §五）。
// 沒有寫成「重用 UBTTask_MoveOnGrid/UBTTask_AttackPlayer」——那兩個既有 Task 內部直接
// `Cast<AEnemy>(AIC->GetPawn())`，是 AEnemy 專屬的，硬套到 ANPCCharacter 只會在執行期
// cast 失敗、永遠 Failed。改成一個自包含的小 Task，邏輯（追擊單步移動 + 進入距離後攻擊）
// 跟 UBTTask_MoveOnGrid/UBTTask_AttackPlayer 同款設計，但獨立成 NPC 自己的版本——符合本
// 專案「三行重複好過早抽象」慣例，不冒險改動已經穩定運作的敵人 AI 程式碼。
UCLASS()
class SKILLCREATORRUNTIME_API UBTTask_NPCCounterAttack : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_NPCCounterAttack();

    // 近戰攻擊距離（棋盤格距離），暫定 2（docs/plan-base-npc-system.md 沒有明確數值）
    UPROPERTY(EditAnywhere, Category="Combat") int32 AttackRangeTiles = 2;

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
