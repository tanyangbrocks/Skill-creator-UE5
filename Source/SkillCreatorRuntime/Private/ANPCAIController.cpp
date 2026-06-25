#include "ANPCAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "ASkillCreatorCharacter.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "WorldScale.h"
#include "MaterialType.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

ANPCAIController::ANPCAIController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ANPCAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (BehaviorTreeAsset)
        RunBehaviorTree(BehaviorTreeAsset);
}

void ANPCAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (BehaviorTreeAsset) return; // BT asset 已接管，不重複驅動

    MoveCooldown -= DeltaTime;
    if (MoveCooldown > 0.f) return;
    MoveCooldown = MoveInterval;

    ANPCCharacter* NPC = Cast<ANPCCharacter>(GetPawn());
    if (!NPC || !NPC->IsAlive()) return;

    FTileWorld3D* TW = NPC->CachedVoxelWorld ? NPC->CachedVoxelWorld->GetTileWorld() : nullptr;
    if (!TW) return;

    if (NPC->bFleeRequested)          StepFlee(NPC, TW);
    else if (NPC->FollowTarget)       StepFollow(NPC, TW);
    else if (NPC->Disposition == ENPCDisposition::Hostile) StepCounterAttack(NPC, TW);
    else                              StepWander(NPC, TW);
}

bool ANPCAIController::TryStep(ANPCCharacter* NPC, FTileWorld3D* TW, int32 Dx, int32 Dz)
{
    if (Dx == 0 && Dz == 0) return false;
    const FGridPos Next(NPC->GridPosition.X + Dx, NPC->GridPosition.Y, NPC->GridPosition.Z + Dz);
    if (TW->GetTile(Next.X, Next.Y, Next.Z) != EMaterialType::Air) return false;
    NPC->GridPosition = Next;
    NPC->SetActorLocation(WorldScale::TileToWorld(Next, NPC->CachedVoxelWorld->WorldHeight));
    return true;
}

void ANPCAIController::StepFlee(ANPCCharacter* NPC, FTileWorld3D* TW)
{
    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player) return;

    const FGridPos PlayerPos = WorldScale::WorldToTile(
        Player->GetActorLocation(), NPC->CachedVoxelWorld->WorldHeight);
    const FGridPos Cur = NPC->GridPosition;

    const int32 Dx = FMath::Sign(Cur.X - PlayerPos.X);
    const int32 Dz = FMath::Sign(Cur.Z - PlayerPos.Z);
    const int32 StepX = (Dx != 0) ? Dx : (FMath::RandBool() ? 1 : -1);
    const int32 StepZ = (Dz != 0) ? Dz : (FMath::RandBool() ? 1 : -1);

    if (!TryStep(NPC, TW, StepX, StepZ))
        if (!TryStep(NPC, TW, StepX, 0))
            TryStep(NPC, TW, 0, StepZ);
}

void ANPCAIController::StepFollow(ANPCCharacter* NPC, FTileWorld3D* TW)
{
    const FGridPos Cur    = NPC->GridPosition;
    const FGridPos Target = WorldScale::WorldToTile(
        NPC->FollowTarget->GetActorLocation(), NPC->CachedVoxelWorld->WorldHeight);

    if (Cur == Target) return;

    const int32 Dx = FMath::Sign(Target.X - Cur.X);
    const int32 Dz = FMath::Sign(Target.Z - Cur.Z);

    if (!TryStep(NPC, TW, Dx, Dz))
        if (!TryStep(NPC, TW, Dx, 0))
            TryStep(NPC, TW, 0, Dz);
}

void ANPCAIController::StepCounterAttack(ANPCCharacter* NPC, FTileWorld3D* TW)
{
    ASkillCreatorCharacter* Player = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();
    for (TActorIterator<ASkillCreatorCharacter> It(GetWorld()); It; ++It)
    {
        if (!It->IsAlive()) continue;
        const float D = NPC->GetPosition().EuclideanDistance(It->GetPosition());
        if (D < BestDistSq) { BestDistSq = D; Player = *It; }
    }
    if (!Player) return;

    const FGridPos NPos = NPC->GetPosition();
    const FGridPos PPos = Player->GetPosition();

    if (NPos.ChebyshevDistance(PPos) <= AttackRangeTiles)
    {
        Player->TakeDirectDamage(NPC->Stats.Power);
        return;
    }

    const int32 Dx = FMath::Sign(PPos.X - NPos.X);
    const int32 Dz = FMath::Sign(PPos.Z - NPos.Z);

    if (!TryStep(NPC, TW, Dx, Dz))
        if (!TryStep(NPC, TW, Dx, 0))
            TryStep(NPC, TW, 0, Dz);
}

void ANPCAIController::StepWander(ANPCCharacter* NPC, FTileWorld3D* TW)
{
    if (!NPC->bHasWanderTarget || NPC->GridPosition == NPC->WanderTarget)
    {
        NPC->bHasWanderTarget = false;
        NPC->WanderRetryTimer -= MoveInterval;
        if (NPC->WanderRetryTimer > 0.f) return;

        const int32 Dx = FMath::RandRange(-WanderRadiusTiles, WanderRadiusTiles);
        const int32 Dz = FMath::RandRange(-WanderRadiusTiles, WanderRadiusTiles);
        const FGridPos Candidate(
            NPC->SpawnGridPos.X + Dx, NPC->GridPosition.Y, NPC->SpawnGridPos.Z + Dz);

        if (TW->GetTile(Candidate.X, Candidate.Y, Candidate.Z) == EMaterialType::Air)
        {
            NPC->WanderTarget     = Candidate;
            NPC->bHasWanderTarget = true;
        }
        NPC->WanderRetryTimer = FMath::FRandRange(MinRetryInterval, MaxRetryInterval);
        return;
    }

    const FGridPos Cur = NPC->GridPosition;
    const int32 Dx = FMath::Sign(NPC->WanderTarget.X - Cur.X);
    const int32 Dz = FMath::Sign(NPC->WanderTarget.Z - Cur.Z);

    bool bMoved = TryStep(NPC, TW, Dx, Dz);
    if (!bMoved) bMoved = TryStep(NPC, TW, Dx, 0);
    if (!bMoved) bMoved = TryStep(NPC, TW, 0, Dz);

    if (!bMoved)
        NPC->bHasWanderTarget = false; // 卡住，放棄這個目標
}
