#include "UBTTask_NPCFollow.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "ANPCCharacter.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "WorldScale.h"
#include "MaterialType.h"
#include "TilePathfinder.h"

UBTTask_NPCFollow::UBTTask_NPCFollow()
{
    NodeName = TEXT("NPC Follow");
}

EBTNodeResult::Type UBTTask_NPCFollow::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* /*NodeMemory*/)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    if (!AIC) return EBTNodeResult::Failed;

    ANPCCharacter* NPC = Cast<ANPCCharacter>(AIC->GetPawn());
    if (!NPC || !NPC->FollowTarget) return EBTNodeResult::Failed;

    FTileWorld3D* TW = NPC->CachedVoxelWorld ? NPC->CachedVoxelWorld->GetTileWorld() : nullptr;
    if (!TW) return EBTNodeResult::Succeeded;

    const int32    WorldH = NPC->CachedVoxelWorld->WorldHeight;
    const FGridPos Cur    = NPC->GridPosition;
    const FGridPos Goal   = WorldScale::WorldToTile(NPC->FollowTarget->GetActorLocation(), WorldH);

    if (Cur == Goal) return EBTNodeResult::Succeeded;

    // 下一步阻塞時強制重算
    const int32 NextStep = NPC->PathStep + 1;
    if (!NPC->bPathDirty && TW && NPC->CachedPath.IsValidIndex(NextStep))
    {
        const FGridPos& NS = NPC->CachedPath[NextStep];
        if (TW->GetTile(NS.X, NS.Y, NS.Z) != EMaterialType::Air)
            NPC->bPathDirty = true;
    }

    if (NPC->CachedGoal != Goal || NPC->bPathDirty || NPC->PathStep >= NPC->CachedPath.Num() - 1)
    {
        NPC->CachedPath  = FTilePathfinder::FindPath({ Cur, Goal, TW, 1024, 1, 8 });
        NPC->CachedGoal  = Goal;
        NPC->PathStep    = 0;
        NPC->bPathDirty  = false;
    }

    if (NPC->CachedPath.Num() < 2) return EBTNodeResult::Succeeded;

    ++NPC->PathStep;
    const FGridPos& Next = NPC->CachedPath[NPC->PathStep];
    NPC->GridPosition = Next;
    NPC->SetActorLocation(WorldScale::TileToWorld(Next, WorldH));

    return EBTNodeResult::Succeeded;
}
