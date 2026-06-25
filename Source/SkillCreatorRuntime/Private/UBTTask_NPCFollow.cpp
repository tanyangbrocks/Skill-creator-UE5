#include "UBTTask_NPCFollow.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "ANPCCharacter.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "WorldScale.h"
#include "MaterialType.h"

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

    const FGridPos Cur    = NPC->GridPosition;
    const FGridPos Target = WorldScale::WorldToTile(
        NPC->FollowTarget->GetActorLocation(), NPC->CachedVoxelWorld->WorldHeight);

    if (Cur == Target) return EBTNodeResult::Succeeded; // already adjacent

    const int32 Dx = FMath::Sign(Target.X - Cur.X);
    const int32 Dz = FMath::Sign(Target.Z - Cur.Z);

    auto TryStep = [&](int32 sx, int32 sz) -> bool
    {
        if (sx == 0 && sz == 0) return false;
        const FGridPos N(Cur.X + sx, Cur.Y, Cur.Z + sz);
        if (TW->GetTile(N.X, N.Y, N.Z) != EMaterialType::Air) return false;
        NPC->GridPosition = N;
        NPC->SetActorLocation(WorldScale::TileToWorld(N, NPC->CachedVoxelWorld->WorldHeight));
        return true;
    };

    if (!TryStep(Dx, Dz))
        if (!TryStep(Dx, 0))
            TryStep(0, Dz);

    return EBTNodeResult::Succeeded;
}
