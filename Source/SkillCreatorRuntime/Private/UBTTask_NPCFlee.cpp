#include "UBTTask_NPCFlee.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "ANPCCharacter.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "WorldScale.h"
#include "MaterialType.h"
#include "Kismet/GameplayStatics.h"

UBTTask_NPCFlee::UBTTask_NPCFlee()
{
    NodeName = TEXT("NPC Flee");
}

EBTNodeResult::Type UBTTask_NPCFlee::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* /*NodeMemory*/)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    if (!AIC) return EBTNodeResult::Failed;

    ANPCCharacter* NPC = Cast<ANPCCharacter>(AIC->GetPawn());
    if (!NPC || !NPC->bFleeRequested) return EBTNodeResult::Failed;

    FTileWorld3D* TW = NPC->CachedVoxelWorld ? NPC->CachedVoxelWorld->GetTileWorld() : nullptr;
    if (!TW) return EBTNodeResult::Succeeded;

    APawn* Player = UGameplayStatics::GetPlayerPawn(OwnerComp.GetWorld(), 0);
    if (!Player) return EBTNodeResult::Succeeded;

    const FGridPos PlayerPos = WorldScale::WorldToTile(
        Player->GetActorLocation(), NPC->CachedVoxelWorld->WorldHeight);
    const FGridPos Cur = NPC->GridPosition;

    // Step one tile in the direction away from the player
    const int32 Dx = FMath::Sign(Cur.X - PlayerPos.X);
    const int32 Dz = FMath::Sign(Cur.Z - PlayerPos.Z);
    const int32 StepX = (Dx != 0) ? Dx : (FMath::RandBool() ? 1 : -1);
    const int32 StepZ = (Dz != 0) ? Dz : (FMath::RandBool() ? 1 : -1);

    auto TryStep = [&](int32 sx, int32 sz) -> bool
    {
        if (sx == 0 && sz == 0) return false;
        const FGridPos N(Cur.X + sx, Cur.Y, Cur.Z + sz);
        if (TW->GetTile(N.X, N.Y, N.Z) != EMaterialType::Air) return false;
        NPC->GridPosition = N;
        NPC->SetActorLocation(WorldScale::TileToWorld(N, NPC->CachedVoxelWorld->WorldHeight));
        return true;
    };

    if (!TryStep(StepX, StepZ))
        if (!TryStep(StepX, 0))
            TryStep(0, StepZ);

    return EBTNodeResult::Succeeded;
}
