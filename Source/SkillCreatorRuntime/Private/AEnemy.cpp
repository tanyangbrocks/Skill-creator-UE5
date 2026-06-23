#include "AEnemy.h"
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

int32 AEnemy::NextId = 0;

AEnemy::AEnemy()
{
    PrimaryActorTick.bCanEverTick = false;
    AuraComp = CreateDefaultSubobject<UElementalAuraComponent>(TEXT("AuraComp"));
    UniqueId = ++NextId;

    // 2026-06-22 修復：對應 Godot Main.cs:1716 CreateEnemyMesh()——每種 EEnemyType 一個
    // 純色 BoxMesh（Unshaded），邊長 = Grain×TileSize = 1 遊戲單位（100cm，與 WorldScale
    // 恆等式一致）。UE5 之前完全沒有任何視覺元件（純 APawn，BP_Enemy 子類也只加了
    // AIControllerClass，沒有 mesh），敵人即使正確 Spawn 在世界裡也完全不可見。
    // 用引擎內建 Cube + 動態材質上色，不需要額外美術資產。
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    SetRootComponent(MeshComp);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComp->SetCollisionObjectType(ECC_Pawn);
    MeshComp->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeFinder.Succeeded())
    {
        MeshComp->SetStaticMesh(CubeFinder.Object);
        // Cube 預設 100×100×100cm；縮放至 1 遊戲單位（Grain×TileSizeCm，恆等於 100cm，
        // 但用公式表示以保持「改 Grain 自動跟進」的專案慣例，對應 WorldScale.h 其他用法）。
        const float UnitCm = WorldScale::TileSizeCm * static_cast<float>(WorldScale::GrainCurrent);
        const float S = UnitCm / 100.f;
        MeshComp->SetRelativeScale3D(FVector(S));
        // Godot mesh.Position.Y = (Position.Y+1)*eT - mh*0.5（腳底格往上半個身高對齊地表，
        // mh = 1 遊戲單位高）；UE5 SetActorLocation 用 TileToWorld（腳底格中心），所以 mesh
        // 相對 root 往上偏移半個單位即可達到同樣的「腳底對齊地表」視覺效果。
        MeshComp->SetRelativeLocation(FVector(0.f, 0.f, UnitCm * 0.5f));
    }

    // 優先用 BP_EnemyAIController（若存在，留給未來在 Blueprint 補額外邏輯的空間）；
    // 找不到時 fallback 到純 C++ AEnemyAIController（其 constructor 自己也會載入
    // /Game/BT_Enemy，所以即使 BP 子類別不存在，AI 仍會正常運作）。
    static ConstructorHelpers::FClassFinder<AAIController> AIControllerBPClass(TEXT("/Game/BP_EnemyAIController"));
    if (AIControllerBPClass.Succeeded())
        AIControllerClass = AIControllerBPClass.Class;
    else
        AIControllerClass = AEnemyAIController::StaticClass();
}

// 對應 Godot Main.cs:1719-1726 CreateEnemyMesh() 的 per-type 顏色表（純 Unshaded 色塊，
// 不靠材質貼圖分辨怪物種類）。動態材質的 Parent 用 mesh 元件當前已指定的材質（不論是
// Cube 自帶的引擎預設材質還是其他），SetVectorParameterValue 對沒有該參數的材質是
// no-op，最差情況維持原色但 mesh 仍然可見——不會因為材質缺參數而整體失敗。
void AEnemy::ApplyBodyColor()
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

void AEnemy::BeginPlay()
{
    Super::BeginPlay();

    ApplyBodyColor();

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

    // 依 Type 設預設 Stats（攻防數值）
    // 這些只是初始值，Editor 或 Manager 可以覆寫
    Stats.MaxHpBase = MaxHp;    // 與 Hp/MaxHp 系統同步上限供 UI/技能公式讀取
    switch (Type)
    {
    case EEnemyType::Heavy:
        Stats.PhysicalDefense = 8.f;
        Stats.CritRate        = 0.02f;
        Stats.Power           = 25.f;
        break;
    case EEnemyType::Ranged:
        Stats.CritRate = 0.10f;
        Stats.Power    = 12.f;  // 投射物 BaseDamage 來源（對應 Godot EnemyManager.cs:486 BoltDamage=12）
        break;
    case EEnemyType::Patrol:
        Stats.DodgeRate       = 0.05f;
        Stats.Power           = 6.f;
        break;
    default:    // Melee
        Stats.Power           = 8.f;
        break;
    }
}

// B-3：物理傷害管線（共用 FCharacterStats::ResolvePhysicalDmg，避免三地 copy-paste）
void AEnemy::TakePhysicalDamage(float PhysAtk, const FCharacterStats* Atk)
{
    const float Final = FCharacterStats::ResolvePhysicalDmg(PhysAtk, Stats, Atk);
    if (Final < 0.f) return;
    TakeDamageAmount(Final);
}

// B-3：能量傷害管線（4 步防禦，與玩家端一致）
void AEnemy::TakeEnergyDamage(float EnergyAtk, FName ManaTypeKey, const FCharacterStats* Atk)
{
    if (Atk)
    {
        if (Atk->HitRate < 1.f && FMath::FRand() > Atk->HitRate) return;
        float ExcessHit = FMath::Max(0.f, Atk->HitRate - 1.f);
        float EffDodge  = FMath::Max(0.f, Stats.DodgeRate - ExcessHit);
        if (FMath::FRand() < EffDodge) return;
    }

    float Step1 = FMath::Max(0.f, EnergyAtk - Stats.GetMpDefense(ManaTypeKey));
    float Step2 = FMath::Max(0.f, Step1      - Stats.EnergyDefense);
    float Step3 = FMath::Max(0.f, Step2      - Stats.GetMpDamageReduction(ManaTypeKey));
    float Final = FMath::Max(0.f, Step3      - Stats.EnergyDamageReduction);

    if (Atk && Final > 0.f)
    {
        float EffCritRate = FMath::Max(0.f, Atk->CritRate - Stats.AntiCrit);
        if (FMath::FRand() < EffCritRate)
        {
            float EffCritMult = FMath::Max(1.f, Atk->CritDmgMult - Stats.AntiCritDmgReduction);
            Final *= EffCritMult;
            float EffSuperRate = FMath::Max(0.f, Atk->SuperCritRate - Stats.AntiSuperCritRate);
            if (FMath::FRand() < EffSuperRate)
            {
                float EffSuperMult = FMath::Max(1.f, Atk->SuperCritDmgMult - Stats.AntiSuperCritDmgReduction);
                Final *= EffSuperMult;
            }
        }
    }

    TakeDamageAmount(Final);
}

void AEnemy::TakeDamageAmount(float Amount)
{
    if (!IsAlive()) return;  // 防止同幀多次傷害在 Hp=0 後繼續執行（掉落物/XP 誤觸發）
    float Modified = Amount * (1.f + AuraComp->DamageTakenBonus) * (1.f + AuraComp->DefensePenalty);

    // 傷害護盾攔截（對應 Godot Enemy.cs:425 ActionBus.Dispatch(EntityDamageAction)）
    Modified = ActionBus.DispatchPlayerDamage(Modified);

    Hp = FMath::Max(0.f, Hp - Modified);

    if (Modified > 0.f)
        AFloatingDamageActor::Spawn(GetWorld(),
            GetActorLocation() + FVector(0.f, 0.f, 50.f), Modified);
}

void AEnemy::Respawn()
{
    // 清除 S-2 攻擊計時器，避免復活後舊計時器觸發相位異常
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
    case EEnemyType::Ranged: return Stats.Power;  // 投射物 BaseDamage（暴擊由 Stats.CritRate 驅動）
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

// ── S-2 Melee 攻擊框架 ──────────────────────────────────────────────────────

void AEnemy::BeginMeleeAttack(ASkillCreatorCharacter* Target)
{
    // 上一次攻擊還未結束，或自身已死亡，忽略
    if (!IsAlive() || AttackPhase != EAttackPhase::None || !Target || !Target->IsAlive()) return;
    MeleeTarget = Target;
    AttackPhase = EAttackPhase::WindingUp;
    // Heavy：0.7s；Patrol（快速近戰）：0.25s；Melee：0.4s
    const float WindupSec = (Type == EEnemyType::Heavy)  ? 0.7f
                          : (Type == EEnemyType::Patrol) ? 0.25f : 0.4f;
    GetWorldTimerManager().SetTimer(WindupTimer, this, &AEnemy::OnWindupEnd, WindupSec, false);
}

void AEnemy::OnWindupEnd()
{
    // 自己死亡（前搖期間被玩家擊殺）→ 中止，Respawn() 已清計時器
    if (!IsAlive()) return;
    AttackPhase = EAttackPhase::Active;
    ASkillCreatorCharacter* Target = MeleeTarget.Get();
    if (Target && Target->IsAlive())
    {
        // 攻擊幀時用 tile 空間 Chebyshev 距離再確認（與 BTTask gate 一致，避免 Euclidean 對角漏打）
        if (GetPosition().ChebyshevDistance(Target->GetPosition()) <= GetAttackRange())
        {
            // 走 B-3 物理傷害管線：支援 S-4 彈反（bInParryWindow）、防禦減傷、暴擊
            Target->TakePhysicalDamage(GetAttackDamage(), &Stats, this);
        }
    }
    // Heavy：0.3s；Patrol：0.15s；Melee：0.2s
    const float ActiveSec = (Type == EEnemyType::Heavy)  ? 0.3f
                          : (Type == EEnemyType::Patrol) ? 0.15f : 0.2f;
    GetWorldTimerManager().SetTimer(ActiveTimer, this, &AEnemy::OnActiveEnd, ActiveSec, false);
}

void AEnemy::OnActiveEnd()
{
    if (!IsAlive()) return;
    AttackPhase = EAttackPhase::Recovering;
    // Heavy：0.6s；Patrol：0.3s；Melee：0.4s
    const float RecoverySec = (Type == EEnemyType::Heavy)  ? 0.6f
                            : (Type == EEnemyType::Patrol) ? 0.3f : 0.4f;
    // UObject 成員函式綁定：TimerManager 在 Actor PendingKill 時自動跳過，比 raw lambda 安全
    GetWorldTimerManager().SetTimer(RecoveryTimer, this, &AEnemy::OnRecoveryEnd, RecoverySec, false);
}

void AEnemy::OnRecoveryEnd()
{
    AttackPhase = EAttackPhase::None;
    MeleeTarget = nullptr;
}
