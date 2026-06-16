#include "AEnemy.h"
#include "UElementalAuraComponent.h"
#include "AEnemyAIController.h"
#include "WorldScale.h"

int32 AEnemy::NextId = 0;

AEnemy::AEnemy()
{
    PrimaryActorTick.bCanEverTick = false;
    AuraComp = CreateDefaultSubobject<UElementalAuraComponent>(TEXT("AuraComp"));
    UniqueId = ++NextId;
    AIControllerClass = AEnemyAIController::StaticClass();
}

void AEnemy::BeginPlay()
{
    Super::BeginPlay();

    // 若 MaxHp 未在 Editor 覆蓋，則依 Type 設預設值
    if (MaxHp <= 0.f)
    {
        MaxHp = [this]() -> float {
            switch (Type)
            {
            case EEnemyType::Heavy:  return 150.f;
            case EEnemyType::Ranged: return 35.f;
            case EEnemyType::Patrol: return 45.f;
            default:                 return 50.f;
            }
        }();
    }
    Hp = MaxHp;

    SpawnGridPos = GridPosition;
}

void AEnemy::TakeDamageAmount(float Amount)
{
    float Modified = Amount * (1.f + AuraComp->DamageTakenBonus) * (1.f + AuraComp->DefensePenalty);
    Hp = FMath::Max(0.f, Hp - Modified);
}

void AEnemy::Respawn()
{
    GridPosition = SpawnGridPos;
    Hp           = MaxHp;
    AIState      = EEnemyState::Idle;
    AuraComp->Reset();
    SetActorLocation(WorldScale::TileToWorld(SpawnGridPos));
}

float AEnemy::GetBaseMoveInterval() const
{
    switch (Type)
    {
    case EEnemyType::Heavy:  return 0.60f;
    case EEnemyType::Ranged: return 0.45f;
    case EEnemyType::Patrol: return 0.40f;
    default:                 return 0.35f;
    }
}

float AEnemy::GetMoveInterval() const
{
    return GetBaseMoveInterval() * (1.f + AuraComp->SpeedPenalty);
}

float AEnemy::GetAttackInterval() const
{
    switch (Type)
    {
    case EEnemyType::Heavy:  return 2.5f;
    case EEnemyType::Ranged: return 2.2f;
    default:                 return 1.8f;
    }
}

float AEnemy::GetAttackDamage() const
{
    switch (Type)
    {
    case EEnemyType::Heavy:  return 25.f;
    case EEnemyType::Ranged: return 0.f;   // 用投射物傷害（M-5）
    default:                 return 8.f;
    }
}

int32 AEnemy::GetAttackRange() const
{
    switch (Type)
    {
    case EEnemyType::Heavy:  return 3;
    case EEnemyType::Ranged: return 12;
    default:                 return 2;
    }
}

int32 AEnemy::GetDetectRange() const
{
    switch (Type)
    {
    case EEnemyType::Ranged: return 30;
    case EEnemyType::Patrol: return 15;
    default:                 return 25;
    }
}

float AEnemy::GetXpReward() const
{
    switch (Type)
    {
    case EEnemyType::Ranged: return 20.f;
    case EEnemyType::Patrol: return 15.f;
    case EEnemyType::Heavy:  return 40.f;
    default:                 return 10.f;
    }
}
