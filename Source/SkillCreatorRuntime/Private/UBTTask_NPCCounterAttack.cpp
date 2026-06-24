#include "UBTTask_NPCCounterAttack.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "ANPCCharacter.h"
#include "ASkillCreatorCharacter.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "WorldScale.h"
#include "MaterialType.h"
#include "EngineUtils.h"

UBTTask_NPCCounterAttack::UBTTask_NPCCounterAttack()
{
    NodeName = TEXT("NPC Counter Attack");
}

EBTNodeResult::Type UBTTask_NPCCounterAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* /*NodeMemory*/)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    if (!AIC) return EBTNodeResult::Failed;

    ANPCCharacter* NPC = Cast<ANPCCharacter>(AIC->GetPawn());
    if (!NPC || !NPC->IsAlive() || NPC->Disposition != ENPCDisposition::Hostile)
        return EBTNodeResult::Failed;

    // 沒有 Blackboard target service（NPC 平時是中立的，不像敵人一直追蹤玩家位置），
    // 反擊時直接掃最近的玩家——敵對狀態通常只維持 HostileCooldownDuration 那麼短的時間，
    // O(玩家數) 掃描的成本可以忽略。
    ASkillCreatorCharacter* Player = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();
    for (TActorIterator<ASkillCreatorCharacter> It(OwnerComp.GetWorld()); It; ++It)
    {
        if (!It->IsAlive()) continue;
        const float D = NPC->GetPosition().EuclideanDistance(It->GetPosition());
        if (D < BestDistSq) { BestDistSq = D; Player = *It; }
    }
    if (!Player) return EBTNodeResult::Failed;

    const FGridPos NPos = NPC->GetPosition();
    const FGridPos PPos = Player->GetPosition();
    const int32 Dist = NPos.ChebyshevDistance(PPos);

    if (Dist <= AttackRangeTiles)
    {
        Player->TakeDirectDamage(NPC->Stats.Power);
        return EBTNodeResult::Succeeded;
    }

    // 追擊：單步往玩家移動（同 UBTTask_WanderRandomly 的單步嘗試邏輯）
    FTileWorld3D* TW = NPC->CachedVoxelWorld ? NPC->CachedVoxelWorld->GetTileWorld() : nullptr;
    if (!TW) return EBTNodeResult::Succeeded;

    FGridPos Cur = NPos;
    const int32 Dx = FMath::Sign(PPos.X - Cur.X);
    const int32 Dz = FMath::Sign(PPos.Z - Cur.Z);

    auto TryStep = [&](int32 StepX, int32 StepZ) -> bool
    {
        if (StepX == 0 && StepZ == 0) return false;
        const FGridPos Next(Cur.X + StepX, Cur.Y, Cur.Z + StepZ);
        if (TW->GetTile(Next.X, Next.Y, Next.Z) != EMaterialType::Air) return false;
        Cur = Next;
        return true;
    };

    if (!TryStep(Dx, Dz))
        if (!TryStep(Dx, 0))
            TryStep(0, Dz);

    if (Cur != NPos)
    {
        NPC->GridPosition = Cur;
        NPC->SetActorLocation(WorldScale::TileToWorld(Cur, NPC->CachedVoxelWorld->WorldHeight));
    }

    return EBTNodeResult::Succeeded;
}
