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

    FTileWorld3D* TW = nullptr;
    if (AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(AIC->GetWorld()))
        TW = VW->GetTileWorld();

    // I-9：四種 AI 行為分支（對應 Godot Enemy.cs:191-340 UpdateMelee/Ranged/Patrol/Heavy）。
    // Manhattan 距離 + AttackRange/DetectRange 門檻，對應 Enemy.cs:177-186 dist 計算
    // （Melee/Heavy 邏輯本身完全相同，差異只在數值，已由 GetAttackRange/GetDetectRange
    // 等 per-type stat 反映，不需要分支）。
    const int32 Dist = FMath::Abs(Goal.X - Start.X) + FMath::Abs(Goal.Y - Start.Y) + FMath::Abs(Goal.Z - Start.Z);
    const int32 AttackRange = Enemy->GetAttackRange();
    const int32 DetectRange = Enemy->GetDetectRange();

    if (Dist <= AttackRange)
        return EBTNodeResult::Succeeded;  // 已進入攻擊距離，交給 Attack 節點，本節點不移動

    // 單步嘗試移動（對應 Enemy.cs:339-376 TryMoveXZ：優先對角，次 X，次 Z）
    auto TryStepXZ = [&](int32 dx, int32 dz) -> bool
    {
        if ((dx == 0 && dz == 0) || !TW) return false;
        if (dx != 0 && dz != 0)
        {
            FIntVector Diag(Start.X + dx, Start.Y, Start.Z + dz);
            if (TW->GetTile(Diag.X, Diag.Y, Diag.Z) == EMaterialType::Air)
            { Start = FGridPos(Diag.X, Diag.Y, Diag.Z); return true; }
        }
        if (dx != 0)
        {
            FIntVector Nx(Start.X + dx, Start.Y, Start.Z);
            if (TW->GetTile(Nx.X, Nx.Y, Nx.Z) == EMaterialType::Air)
            { Start = FGridPos(Nx.X, Nx.Y, Nx.Z); return true; }
        }
        if (dz != 0)
        {
            FIntVector Nz(Start.X, Start.Y, Start.Z + dz);
            if (TW->GetTile(Nz.X, Nz.Y, Nz.Z) == EMaterialType::Air)
            { Start = FGridPos(Nz.X, Nz.Y, Nz.Z); return true; }
        }
        return false;
    };

    if (Dist > DetectRange)
    {
        if (Enemy->Type != EEnemyType::Patrol)
            return EBTNodeResult::Succeeded;  // Melee/Ranged/Heavy 超出偵測範圍 → Idle，原地不動

        // Patrol：沿出生點 X 軸 ±PatrolRange 來回巡邏（對應 Enemy.cs:267-298 PatrolRange=12）
        constexpr int32 PatrolRange = 12;
        if (TryStepXZ(Enemy->PatrolDir, 0))
        {
            Enemy->GridPosition = Start;
            Enemy->SetActorLocation(FVector((float)Start.X, (float)Start.Y, (float)Start.Z));
            if (FMath::Abs(Start.X - Enemy->SpawnGridPos.X) >= PatrolRange)
                Enemy->PatrolDir = -Enemy->PatrolDir;
        }
        else
        {
            Enemy->PatrolDir = -Enemy->PatrolDir;
        }
        return EBTNodeResult::Succeeded;
    }

    if (Enemy->Type == EEnemyType::Ranged)
    {
        // Ranged：太近時後退（對應 Enemy.cs:222-234 RangedPreferredDist=8）
        constexpr int32 RangedPreferredDist = 8;
        if (Dist < RangedPreferredDist)
        {
            const int32 dx = FMath::Sign(Start.X - Goal.X);
            const int32 dz = FMath::Sign(Start.Z - Goal.Z);
            if (TryStepXZ(dx, dz))
            {
                Enemy->GridPosition = Start;
                Enemy->SetActorLocation(FVector((float)Start.X, (float)Start.Y, (float)Start.Z));
            }
            return EBTNodeResult::Succeeded;
        }
    }

    // ── 一般追擊：BFS（XZ 平面，固定 Y = 敵人當前高度）── Melee/Heavy 預設、
    // Ranged 在 [RangedPreferredDist, DetectRange] 區間內走這條路（對應 Enemy.cs:248-264）
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
