#include "UBTTask_WanderRandomly.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "ANPCCharacter.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "WorldScale.h"
#include "MaterialType.h"

UBTTask_WanderRandomly::UBTTask_WanderRandomly()
{
    NodeName = TEXT("Wander Randomly");
}

EBTNodeResult::Type UBTTask_WanderRandomly::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* /*NodeMemory*/)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    if (!AIC) return EBTNodeResult::Failed;

    ANPCCharacter* NPC = Cast<ANPCCharacter>(AIC->GetPawn());
    if (!NPC) return EBTNodeResult::Failed;

    // 敵對中：交給反擊分支，本節點不接管移動（docs/plan-base-npc-system.md §五）
    if (NPC->Disposition == ENPCDisposition::Hostile)
        return EBTNodeResult::Failed;

    FTileWorld3D* TW = NPC->CachedVoxelWorld ? NPC->CachedVoxelWorld->GetTileWorld() : nullptr;
    if (!TW) return EBTNodeResult::Succeeded; // 沒有世界可走，原地等待

    // 還沒有目標、或已抵達目標：等待重試計時器，計時到了重新抽一個目標
    if (!NPC->bHasWanderTarget || NPC->GridPosition == NPC->WanderTarget)
    {
        NPC->bHasWanderTarget = false;
        NPC->WanderRetryTimer -= OwnerComp.GetWorld()->GetDeltaSeconds();
        if (NPC->WanderRetryTimer > 0.f) return EBTNodeResult::Succeeded;

        const int32 Dx = FMath::RandRange(-WanderRadiusTiles, WanderRadiusTiles);
        const int32 Dz = FMath::RandRange(-WanderRadiusTiles, WanderRadiusTiles);
        const FGridPos Candidate(NPC->SpawnGridPos.X + Dx, NPC->GridPosition.Y, NPC->SpawnGridPos.Z + Dz);

        if (TW->GetTile(Candidate.X, Candidate.Y, Candidate.Z) == EMaterialType::Air)
        {
            NPC->WanderTarget    = Candidate;
            NPC->bHasWanderTarget = true;
        }
        NPC->WanderRetryTimer = FMath::FRandRange(MinRetryInterval, MaxRetryInterval);
        return EBTNodeResult::Succeeded;
    }

    // 單步往目標移動（X 軸優先、再 Z 軸；同 UBTTask_MoveOnGrid 的單步嘗試邏輯）
    FGridPos Cur = NPC->GridPosition;
    const int32 Dx = FMath::Sign(NPC->WanderTarget.X - Cur.X);
    const int32 Dz = FMath::Sign(NPC->WanderTarget.Z - Cur.Z);

    auto TryStep = [&](int32 StepX, int32 StepZ) -> bool
    {
        if (StepX == 0 && StepZ == 0) return false;
        const FGridPos Next(Cur.X + StepX, Cur.Y, Cur.Z + StepZ);
        if (TW->GetTile(Next.X, Next.Y, Next.Z) != EMaterialType::Air) return false;
        Cur = Next;
        return true;
    };

    if (!TryStep(Dx, Dz)) // 對角卡住 → 試單軸
        if (!TryStep(Dx, 0))
            TryStep(0, Dz);

    if (Cur != NPC->GridPosition)
    {
        NPC->GridPosition = Cur;
        NPC->SetActorLocation(WorldScale::TileToWorld(Cur, NPC->CachedVoxelWorld->WorldHeight));
    }
    else
    {
        // 卡住走不過去，放棄這個目標，下一次重新抽
        NPC->bHasWanderTarget = false;
    }

    return EBTNodeResult::Succeeded;
}
