#include "AEnemy.h"
#include "UElementalAuraComponent.h"
#include "AEnemyAIController.h"
#include "AVoxelWorldActor.h"
#include "WorldScale.h"
#include "EngineUtils.h"

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

    for (TActorIterator<AVoxelWorldActor> It(GetWorld()); It; ++It)
    {
        CachedVoxelWorld = *It;
        break;
    }

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
    bPendingRespawn = false;
    GridPosition    = SpawnGridPos;
    Hp              = MaxHp;
    AIState         = EEnemyState::Idle;
    AuraComp->Reset();
    const int32 WH = CachedVoxelWorld ? CachedVoxelWorld->GetTileWorld()->Height
                                      : WorldScale::DefaultWorldHeight;
    SetActorLocation(WorldScale::TileToWorld(SpawnGridPos, WH));
}

void AEnemy::StartRespawn(float DelaySeconds)
{
    if (bPendingRespawn) return;
    bPendingRespawn = true;
    GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AEnemy::Respawn, DelaySeconds, false);
}

void AEnemy::ForceRevive()
{
    GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
    bPendingRespawn = false;
    Respawn();
}

void AEnemy::ApplyGravity()
{
    if (!CachedVoxelWorld) return;
    FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld();
    if (!TW) return;

    // 每次呼叫向下移動一格（由 AIController 每幀決定是否呼叫）
    int32 Below = GridPosition.Y + 1;
    if (TW->InBounds(GridPosition.X, Below, GridPosition.Z)
        && TW->GetTile(GridPosition.X, Below, GridPosition.Z) == EMaterialType::Air)
    {
        GridPosition.Y = Below;
        SetActorLocation(WorldScale::TileToWorld(GridPosition, TW->Height));
    }
}

FEntitySnapshot AEnemy::TakeSnapshot() const
{
    FEntitySnapshot Snap;
    Snap.EntityId  = UniqueId;
    Snap.Position  = GridPosition;
    Snap.Hp        = Hp;
    Snap.Mp        = 0.f;
    Snap.bWasAlive = IsAlive();
    return Snap;
}

void AEnemy::RestoreFromSnapshot(const FEntitySnapshot& Snap)
{
    GridPosition = Snap.Position;
    Hp           = Snap.Hp;
    if (Snap.bWasAlive && !IsAlive())
        ForceRevive();
    SetActorLocation(WorldScale::TileToWorld(GridPosition,
        CachedVoxelWorld ? CachedVoxelWorld->GetTileWorld()->Height : WorldScale::DefaultWorldHeight));
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
