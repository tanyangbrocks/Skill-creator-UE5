#include "AEnemy.h"
#include "UElementalAuraComponent.h"
#include "AEnemyAIController.h"
#include "AVoxelWorldActor.h"
#include "WorldScale.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

int32 AEnemy::NextId = 0;

AEnemy::AEnemy()
{
    PrimaryActorTick.bCanEverTick = false;
    AuraComp = CreateDefaultSubobject<UElementalAuraComponent>(TEXT("AuraComp"));
    UniqueId = ++NextId;

    // 優先用 BP_EnemyAIController（若存在，留給未來在 Blueprint 補額外邏輯的空間）；
    // 找不到時 fallback 到純 C++ AEnemyAIController（其 constructor 自己也會載入
    // /Game/BT_Enemy，所以即使 BP 子類別不存在，AI 仍會正常運作）。
    static ConstructorHelpers::FClassFinder<AAIController> AIControllerBPClass(TEXT("/Game/BP_EnemyAIController"));
    if (AIControllerBPClass.Succeeded())
        AIControllerClass = AIControllerBPClass.Class;
    else
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

    int32 Below = GridPosition.Y + 1;

    if (Type == EEnemyType::Heavy)
    {
        // Heavy 佔 2×2 footprint：只要任一腳下有固體就撐住，全部為 Air 才落下
        bool bSupported = false;
        for (int32 DX = 0; DX <= 1 && !bSupported; ++DX)
            for (int32 DZ = 0; DZ <= 1 && !bSupported; ++DZ)
                if (TW->InBounds(GridPosition.X + DX, Below, GridPosition.Z + DZ) &&
                    TW->GetTile(GridPosition.X + DX, Below, GridPosition.Z + DZ) != EMaterialType::Air)
                    bSupported = true;
        if (!bSupported)
        {
            GridPosition.Y = Below;
            SetActorLocation(WorldScale::TileToWorld(GridPosition, TW->Height));
        }
    }
    else
    {
        if (TW->InBounds(GridPosition.X, Below, GridPosition.Z) &&
            TW->GetTile(GridPosition.X, Below, GridPosition.Z) == EMaterialType::Air)
        {
            GridPosition.Y = Below;
            SetActorLocation(WorldScale::TileToWorld(GridPosition, TW->Height));
        }
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
