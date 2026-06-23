#include "UBTTask_AttackPlayer.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "AEnemy.h"
#include "AEnemyManager.h"
#include "ASpellProjectile.h"
#include "AVoxelWorldActor.h"
#include "ASkillCreatorCharacter.h"
#include "EngineUtils.h"

UBTTask_AttackPlayer::UBTTask_AttackPlayer()
{
    NodeName = TEXT("Attack Player");
}

EBTNodeResult::Type UBTTask_AttackPlayer::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIC = OwnerComp.GetAIOwner();
    if (!AIC) return EBTNodeResult::Failed;

    AEnemy* Enemy = Cast<AEnemy>(AIC->GetPawn());
    if (!Enemy || !Enemy->IsAlive()) return EBTNodeResult::Failed;

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    // Melee：S-2 框架（前搖→攻擊幀→後搖）；Ranged：投射物；其餘：暫時直接傷害 stub
    ASkillCreatorCharacter* Player = nullptr;
    if (UObject* Obj = BB->GetValueAsObject(PlayerKey.SelectedKeyName))
        Player = Cast<ASkillCreatorCharacter>(Obj);

    if (!Player || !Player->IsAlive()) return EBTNodeResult::Failed;

    int32 Range = Enemy->GetAttackRange();
    FGridPos EPos = Enemy->GetPosition();
    FGridPos PPos = Player->GetPosition();
    int32 Dist = EPos.ChebyshevDistance(PPos);  // 棋盤格距離：對角線算 1 格，與 Manhattan 不同

    if (Dist > Range) return EBTNodeResult::Failed;

    if (Enemy->Type == EEnemyType::Ranged)
    {
        // Ranged：生成朝向玩家的投射物
        UWorld* W = OwnerComp.GetWorld();
        AEnemyManager* EnemyMgr = nullptr;
        for (TActorIterator<AEnemyManager> It(W); It; ++It) { EnemyMgr = *It; break; }
        AVoxelWorldActor* VoxelWorld = AVoxelWorldActor::FindInWorld(W);

        if (EnemyMgr && VoxelWorld)
        {
            FVector Dir = FVector(PPos.X - EPos.X, PPos.Y - EPos.Y, PPos.Z - EPos.Z);
            if (!Dir.IsNearlyZero())
                Dir.Normalize();
            else
                Dir = FVector(1.f, 0.f, 0.f);

            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ASpellProjectile* Proj = W->SpawnActor<ASpellProjectile>(
                ASpellProjectile::StaticClass(),
                Enemy->GetActorLocation(), FRotator::ZeroRotator, Params);
            if (Proj)
            {
                Proj->BaseDamage       = Enemy->GetAttackDamage();  // M-1: Stats.Power（非固定常數）
                Proj->PlayerTarget     = Player;
                Proj->AttackerStats    = Enemy->Stats;               // C-1: 值拷貝，避免 Enemy 死亡後懸空
                Proj->bUseAttackerStats = true;
                Proj->OriginEnemy      = Enemy;                      // C-2: S-4 彈反凍結真正的敵人
                Proj->Init(EPos, Dir, EnemyMgr, VoxelWorld);
                EnemyMgr->EnemyProjectiles.Add(Proj);
            }
        }
    }
    else
    {
        // Melee / Heavy / Patrol：前搖→攻擊幀距離確認→B-3 管線（含 S-4 彈反）→後搖
        // 各型計時器在 BeginMeleeAttack 內依 Type 自動調整
        Enemy->BeginMeleeAttack(Player);
    }
    return EBTNodeResult::Succeeded;
}
