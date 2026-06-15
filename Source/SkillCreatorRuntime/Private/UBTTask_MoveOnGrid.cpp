#include "UBTTask_MoveOnGrid.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "AEnemy.h"
#include "ASkillCreatorCharacter.h"
#include "AVoxelWorldActor.h"
#include "MaterialType.h"
#include "Algo/Reverse.h"

UBTTask_MoveOnGrid::UBTTask_MoveOnGrid()
{
    NodeName = TEXT("Move On Grid");
    PlayerKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveOnGrid, PlayerKey), APawn::StaticClass());
}

EBTNodeResult::Type UBTTask_MoveOnGrid::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    if (!AIC) return EBTNodeResult::Failed;

    AEnemy* Enemy = Cast<AEnemy>(AIC->GetPawn());
    if (!Enemy) return EBTNodeResult::Failed;

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    ASkillCreatorCharacter* Player = Cast<ASkillCreatorCharacter>(
        BB->GetValueAsObject(PlayerKey.SelectedKeyName));
    if (!Player || !Player->IsAlive()) return EBTNodeResult::Failed;

    FGridPos Start = Enemy->GetPosition();
    FGridPos Goal  = Player->GetPosition();
    if (Start == Goal) return EBTNodeResult::Succeeded;

    // ── BFS（XZ 平面，固定 Y = 敵人當前高度）─────────────────────
    FTileWorld3D* TW = nullptr;
    if (AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(AIC->GetWorld()))
        TW = VW->GetTileWorld();

    const int32 MaxIter = 512;
    const int32 EY      = Start.Y;

    // CameFrom: key = (X,Z), value = parent (X,Z)
    // FIntPoint has GetTypeHash in CoreMinimal
    TMap<FIntPoint, FIntPoint> CameFrom;
    TArray<FGridPos>            Queue;
    Queue.Reserve(MaxIter);
    Queue.Add(Start);
    CameFrom.Add(FIntPoint(Start.X, Start.Z), FIntPoint(Start.X, Start.Z));

    bool  bFound = false;
    int32 QHead  = 0;

    const int32 DX[4] = { 1, -1, 0,  0 };
    const int32 DZ[4] = { 0,  0, 1, -1 };

    while (QHead < Queue.Num() && QHead < MaxIter)
    {
        FGridPos Cur = Queue[QHead++];

        if (Cur.X == Goal.X && Cur.Z == Goal.Z)
        {
            bFound = true;
            break;
        }

        for (int32 i = 0; i < 4; ++i)
        {
            int32    NX  = Cur.X + DX[i];
            int32    NZ  = Cur.Z + DZ[i];
            FIntPoint NK(NX, NZ);

            if (CameFrom.Contains(NK)) continue;

            // 目標格一律可達；否則查 tile 是否為 Air（無 TileWorld 時忽略碰撞）
            bool bPassable = (NX == Goal.X && NZ == Goal.Z)
                          || (!TW)
                          || (TW->GetTile(NX, EY, NZ) == EMaterialType::Air);
            if (!bPassable) continue;

            CameFrom.Add(NK, FIntPoint(Cur.X, Cur.Z));
            Queue.Add(FGridPos(NX, EY, NZ));
        }
    }

    if (!bFound) return EBTNodeResult::Succeeded;   // 找不到路徑，原地等待

    // ── 從 CameFrom 反推路徑 ─────────────────────────────────────
    TArray<FGridPos> Path;
    {
        int32 CX = Goal.X, CZ = Goal.Z;
        while (!(CX == Start.X && CZ == Start.Z))
        {
            Path.Add(FGridPos(CX, EY, CZ));
            FIntPoint Prev = CameFrom[FIntPoint(CX, CZ)];
            CX = Prev.X;
            CZ = Prev.Y;
        }
        Path.Add(Start);
        Algo::Reverse(Path);
    }

    // ── 沿路徑移動 StepsPerExecution 格 ──────────────────────────
    int32 Steps = FMath::Min(StepsPerExecution, Path.Num() - 1);
    for (int32 s = 0; s < Steps; ++s)
    {
        const FGridPos& NextPos = Path[s + 1];
        Enemy->GridPosition = NextPos;
        Enemy->SetActorLocation(FVector((float)NextPos.X, (float)NextPos.Y, (float)NextPos.Z));
    }

    return EBTNodeResult::Succeeded;
}
