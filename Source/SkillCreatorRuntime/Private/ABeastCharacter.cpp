#include "ABeastCharacter.h"
#include "UElementalAuraComponent.h"
#include "AEnemyAIController.h"
#include "AVoxelWorldActor.h"
#include "WorldScale.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "AFloatingDamageActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ASkillCreatorCharacter.h"
#include "FCombatResolver.h"
#include "UCombatantRegistrySubsystem.h"

int32 ABeastCharacter::NextId = 0;

ABeastCharacter::ABeastCharacter()
{
    PrimaryActorTick.bCanEverTick = false;
    AuraComp = CreateDefaultSubobject<UElementalAuraComponent>(TEXT("AuraComp"));
    UniqueId = ++NextId;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    SetRootComponent(MeshComp);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComp->SetCollisionObjectType(ECC_Pawn);
    MeshComp->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeFinder.Succeeded())
    {
        MeshComp->SetStaticMesh(CubeFinder.Object);
        const float UnitCm = WorldScale::TileSizeCm * static_cast<float>(WorldScale::GrainCurrent);
        const float S = UnitCm / 100.f;
        MeshComp->SetRelativeScale3D(FVector(S));
        MeshComp->SetRelativeLocation(FVector(0.f, 0.f, UnitCm * 0.5f));
    }

    static ConstructorHelpers::FClassFinder<AAIController> AIControllerBPClass(TEXT("/Game/BP_EnemyAIController"));
    if (AIControllerBPClass.Succeeded())
        AIControllerClass = AIControllerBPClass.Class;
    else
        AIControllerClass = AEnemyAIController::StaticClass();
}

void ABeastCharacter::ApplyBodyColor()
{
    if (!MeshComp) return;

    FLinearColor Col;
    switch (Type)
    {
    case EEnemyType::Melee:  Col = FLinearColor(0.90f, 0.15f, 0.15f); break; // 紅
    case EEnemyType::Ranged: Col = FLinearColor(0.90f, 0.50f, 0.10f); break; // 橙
    case EEnemyType::Patrol: Col = FLinearColor(0.35f, 0.25f, 0.80f); break; // 藍紫
    case EEnemyType::Heavy:  Col = FLinearColor(0.55f, 0.08f, 0.08f); break; // 暗紅
    default:                 Col = FLinearColor(0.60f, 0.60f, 0.60f); break;
    }

    UMaterialInterface* Base = MeshComp->GetMaterial(0);
    if (!Base) return;
    UMaterialInstanceDynamic* MID = MeshComp->CreateAndSetMaterialInstanceDynamic(0);
    if (!MID) return;
    MID->SetVectorParameterValue(TEXT("Color"),     Col);
    MID->SetVectorParameterValue(TEXT("BaseColor"), Col);
    MID->SetVectorParameterValue(TEXT("Tint"),      Col);
}

void ABeastCharacter::BeginPlay()
{
    Super::BeginPlay();

    ApplyBodyColor();

    for (TActorIterator<AVoxelWorldActor> It(GetWorld()); It; ++It)
    {
        CachedVoxelWorld = *It;
        break;
    }

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

    if (UCombatantRegistrySubsystem* Reg = GetWorld()->GetSubsystem<UCombatantRegistrySubsystem>())
        Reg->Register(this);

    Stats.MaxHpBase = MaxHp;
    switch (Type)
    {
    case EEnemyType::Heavy:
        Stats.PhysicalDefense = 8.f;
        Stats.CritRate        = 0.02f;
        Stats.Power           = 25.f;
        break;
    case EEnemyType::Ranged:
        Stats.CritRate = 0.10f;
        Stats.Power    = 12.f;
        break;
    case EEnemyType::Patrol:
        Stats.DodgeRate = 0.05f;
        Stats.Power     = 6.f;
        break;
    default:
        Stats.Power = 8.f;
        break;
    }
}

// ── ICombatant 方法 ─────────────────────────────────────────────────────────

UElementalAuraComponent* ABeastCharacter::GetAuraComp() const { return AuraComp; }

bool ABeastCharacter::OccupiesTile(FGridPos Pos) const
{
    if (GetPosition() == Pos) return true;
    if (Type == EEnemyType::Heavy)
    {
        const FGridPos EP = GetPosition();
        if (Pos.Y == EP.Y)
            for (int32 DX = 0; DX <= 1; ++DX)
                for (int32 DZ = 0; DZ <= 1; ++DZ)
                    if (EP.X + DX == Pos.X && EP.Z + DZ == Pos.Z)
                        return true;
    }
    return false;
}

// B-3 管線：FCombatResolver 統一公式 → ApplyFinalDamage → TakeDamageAmount
void ABeastCharacter::TakePhysicalDamage(float PhysAtk, const FCharacterStats* Atk, AActor* /*Attacker*/)
{
    FCombatResolver::TakePhysicalDamage(*this, PhysAtk, Atk);
}

void ABeastCharacter::TakeEnergyDamage(float EnergyAtk, FName ManaTypeKey, const FCharacterStats* Atk)
{
    FCombatResolver::TakeEnergyDamage(*this, EnergyAtk, ManaTypeKey, Atk);
}

void ABeastCharacter::TakeElementalDamage(float ElemAtk, ESkillElementType Element, bool bEnergyDefenseApplies, const FCharacterStats* Atk)
{
    FCombatResolver::TakeElementalDamage(*this, ElemAtk, Element, bEnergyDefenseApplies, Atk);
}

void ABeastCharacter::TakeDamageAmount(float Amount)
{
    if (!IsAlive()) return;
    float Modified = Amount * (1.f + AuraComp->DamageTakenBonus) * (1.f + AuraComp->DefensePenalty);
    Modified = ActionBus.DispatchPlayerDamage(Modified);
    Hp = FMath::Max(0.f, Hp - Modified);
    if (Modified > 0.f)
        AFloatingDamageActor::Spawn(GetWorld(),
            GetActorLocation() + FVector(0.f, 0.f, 50.f), Modified);
}

void ABeastCharacter::EndPlay(EEndPlayReason::Type Reason)
{
    if (UCombatantRegistrySubsystem* Reg = GetWorld() ? GetWorld()->GetSubsystem<UCombatantRegistrySubsystem>() : nullptr)
        Reg->Unregister(this);
    Super::EndPlay(Reason);
}

void ABeastCharacter::Respawn()
{
    GetWorldTimerManager().ClearTimer(WindupTimer);
    GetWorldTimerManager().ClearTimer(ActiveTimer);
    GetWorldTimerManager().ClearTimer(RecoveryTimer);
    AttackPhase = EAttackPhase::None;

    bPendingRespawn = false;
    GridPosition    = SpawnGridPos;
    Hp              = MaxHp;
    AIState         = EEnemyState::Idle;
    AuraComp->Reset();
    const int32 WH = CachedVoxelWorld ? CachedVoxelWorld->GetTileWorld()->Height
                                      : WorldScale::DefaultWorldHeight;
    SetActorLocation(WorldScale::TileToWorld(SpawnGridPos, WH));
}

void ABeastCharacter::StartRespawn(float DelaySeconds)
{
    if (bPendingRespawn) return;
    bPendingRespawn = true;
    GetWorldTimerManager().SetTimer(RespawnTimerHandle, this,
        &ABeastCharacter::Respawn, DelaySeconds, false);
}

void ABeastCharacter::ForceRevive()
{
    GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
    bPendingRespawn = false;
    Respawn();
}

void ABeastCharacter::ApplyGravity()
{
    if (!CachedVoxelWorld) return;
    FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld();
    if (!TW) return;

    int32 Below = GridPosition.Y + 1;

    if (Type == EEnemyType::Heavy)
    {
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

FEntitySnapshot ABeastCharacter::TakeSnapshot() const
{
    FEntitySnapshot Snap;
    Snap.EntityId  = UniqueId;
    Snap.Position  = GridPosition;
    Snap.Hp        = Hp;
    Snap.Mp        = 0.f;
    Snap.bWasAlive = IsAlive();
    return Snap;
}

void ABeastCharacter::RestoreFromSnapshot(const FEntitySnapshot& Snap)
{
    GridPosition = Snap.Position;
    Hp           = Snap.Hp;
    if (Snap.bWasAlive && !IsAlive())
        ForceRevive();
    SetActorLocation(WorldScale::TileToWorld(GridPosition,
        CachedVoxelWorld ? CachedVoxelWorld->GetTileWorld()->Height : WorldScale::DefaultWorldHeight));
}

float ABeastCharacter::GetBaseMoveInterval() const
{
    switch (Type)
    {
    case EEnemyType::Heavy:  return 0.60f;
    case EEnemyType::Ranged: return 0.45f;
    case EEnemyType::Patrol: return 0.40f;
    default:                 return 0.35f;
    }
}

float ABeastCharacter::GetMoveInterval() const { return GetBaseMoveInterval() * (1.f + AuraComp->SpeedPenalty); }

float ABeastCharacter::GetAttackInterval() const
{
    switch (Type)
    {
    case EEnemyType::Heavy:  return 2.5f;
    case EEnemyType::Ranged: return 2.2f;
    default:                 return 1.8f;
    }
}

float ABeastCharacter::GetAttackDamage() const
{
    switch (Type)
    {
    case EEnemyType::Heavy:  return 25.f;
    case EEnemyType::Ranged: return Stats.Power;
    default:                 return 8.f;
    }
}

int32 ABeastCharacter::GetAttackRange() const
{
    switch (Type)
    {
    case EEnemyType::Heavy:  return 3;
    case EEnemyType::Ranged: return 12;
    default:                 return 2;
    }
}

int32 ABeastCharacter::GetDetectRange() const
{
    switch (Type)
    {
    case EEnemyType::Ranged: return 30;
    case EEnemyType::Patrol: return 15;
    default:                 return 25;
    }
}

float ABeastCharacter::GetXpReward() const
{
    switch (Type)
    {
    case EEnemyType::Ranged: return 20.f;
    case EEnemyType::Patrol: return 15.f;
    case EEnemyType::Heavy:  return 40.f;
    default:                 return 10.f;
    }
}

void ABeastCharacter::BeginMeleeAttack(ASkillCreatorCharacter* Target)
{
    if (!IsAlive() || AttackPhase != EAttackPhase::None || !Target || !Target->IsAlive()) return;
    MeleeTarget = Target;
    AttackPhase = EAttackPhase::WindingUp;
    const float WindupSec = (Type == EEnemyType::Heavy)  ? 0.7f
                          : (Type == EEnemyType::Patrol) ? 0.25f : 0.4f;
    GetWorldTimerManager().SetTimer(WindupTimer, this, &ABeastCharacter::OnWindupEnd, WindupSec, false);
}

void ABeastCharacter::OnWindupEnd()
{
    if (!IsAlive()) return;
    AttackPhase = EAttackPhase::Active;
    ASkillCreatorCharacter* Target = MeleeTarget.Get();
    if (Target && Target->IsAlive())
    {
        if (GetPosition().ChebyshevDistance(Target->GetPosition()) <= GetAttackRange())
        {
            Target->TakePhysicalDamage(GetAttackDamage(), &Stats, this);
            // 近戰接觸：玩家 NativeElement → 本體（雙向；攻擊者→玩家由 TakePhysicalDamage 處理）
            if (Target->AuraComp && Target->AuraComp->NativeElement != ESkillElementType::None && AuraComp)
                AuraComp->Apply(Target->AuraComp->NativeElement,
                    UElementalAuraComponent::DefaultAuraDuration, this);
        }
    }
    const float ActiveSec = (Type == EEnemyType::Heavy)  ? 0.3f
                          : (Type == EEnemyType::Patrol) ? 0.15f : 0.2f;
    GetWorldTimerManager().SetTimer(ActiveTimer, this, &ABeastCharacter::OnActiveEnd, ActiveSec, false);
}

void ABeastCharacter::OnActiveEnd()
{
    if (!IsAlive()) return;
    AttackPhase = EAttackPhase::Recovering;
    const float RecoverySec = (Type == EEnemyType::Heavy)  ? 0.6f
                            : (Type == EEnemyType::Patrol) ? 0.3f : 0.4f;
    GetWorldTimerManager().SetTimer(RecoveryTimer, this, &ABeastCharacter::OnRecoveryEnd, RecoverySec, false);
}

void ABeastCharacter::OnRecoveryEnd()
{
    AttackPhase = EAttackPhase::None;
    MeleeTarget = nullptr;
}
