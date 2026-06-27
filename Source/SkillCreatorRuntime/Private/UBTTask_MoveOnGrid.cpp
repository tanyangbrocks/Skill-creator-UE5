#include "UBTTask_MoveOnGrid.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "ABeastCharacter.h"
#include "ASkillCreatorCharacter.h"
#include "AVoxelWorldActor.h"
#include "MaterialType.h"
#include "WorldScale.h"
#include "TilePathfinder.h"

UBTTask_MoveOnGrid::UBTTask_MoveOnGrid()
{
    NodeName = TEXT("Move On Grid");
    PlayerKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveOnGrid, PlayerKey), APawn::StaticClass());
}

EBTNodeResult::Type UBTTask_MoveOnGrid::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    if (!AIC) return EBTNodeResult::Failed;

    ABeastCharacter* Enemy = Cast<ABeastCharacter>(AIC->GetPawn());
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
    int32 WorldH = WorldScale::DefaultWorldHeight;
    if (AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(AIC->GetWorld()))
    {
        TW = VW->GetTileWorld();
        if (TW) WorldH = TW->Height;
    }

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
            Enemy->SetActorLocation(WorldScale::TileToWorld(Start, WorldH));
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
                Enemy->SetActorLocation(WorldScale::TileToWorld(Start, WorldH));
            }
            return EBTNodeResult::Succeeded;
        }
    }

    // ── 一般追擊：3D A*（對應 Enemy.cs:248-264）────────────────────────────────
    // 路徑快取：目標未變且路徑未失效時沿快取走，不重算
    // 下一步 tile 不再可通行（玩家放置方塊或地形塌陷）→ 強制重算
    const int32 NextStep = Enemy->PathStep + 1;
    if (!Enemy->bPathDirty && TW && Enemy->CachedPath.IsValidIndex(NextStep))
    {
        const FGridPos& NS = Enemy->CachedPath[NextStep];
        if (TW->GetTile(NS.X, NS.Y, NS.Z) != EMaterialType::Air)
            Enemy->bPathDirty = true;
    }

    const bool bGoalChanged = (Enemy->CachedGoal != Goal);
    if (bGoalChanged || Enemy->bPathDirty || Enemy->PathStep >= Enemy->CachedPath.Num() - 1)
    {
        const FTilePathRequest Req{ Start, Goal, TW, 1024, /*JumpHeight=*/1, /*MaxFall=*/8 };
        Enemy->CachedPath  = TW ? FTilePathfinder::FindPath(Req) : TArray<FGridPos>{};
        Enemy->CachedGoal  = Goal;
        Enemy->PathStep    = 0;
        Enemy->bPathDirty  = false;
    }

    if (Enemy->CachedPath.Num() < 2) return EBTNodeResult::Succeeded;  // 找不到路徑，原地等待

    // ── 沿快取路徑移動 StepsPerExecution 格 ──────────────────────
    const int32 Steps = FMath::Min(StepsPerExecution, Enemy->CachedPath.Num() - 1 - Enemy->PathStep);
    for (int32 s = 0; s < Steps; ++s)
    {
        ++Enemy->PathStep;
        const FGridPos& NextPos = Enemy->CachedPath[Enemy->PathStep];
        Enemy->GridPosition = NextPos;
        Enemy->SetActorLocation(WorldScale::TileToWorld(NextPos, WorldH));
    }

    return EBTNodeResult::Succeeded;
}
