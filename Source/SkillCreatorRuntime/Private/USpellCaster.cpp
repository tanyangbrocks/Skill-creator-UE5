#include "USpellCaster.h"
#include "ASkillCreatorCharacter.h"
#include "AEnemy.h"
#include "AEnemyManager.h"
#include "ASpellProjectile.h"
#include "AVoxelWorldActor.h"
#include "UCombatStateSubsystem.h"
#include "UElementalAuraComponent.h"
#include "ExecutionContext.h"
#include "MaterialType.h"
#include "WorldScale.h"
#include "EngineUtils.h"

USpellCaster::USpellCaster()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USpellCaster::BeginPlay()
{
    Super::BeginPlay();

    for (TActorIterator<AEnemyManager> It(GetWorld()); It; ++It) { CachedEnemyMgr = *It; break; }
    CachedVoxelWorld = AVoxelWorldActor::FindInWorld(GetWorld());

    // ── SpellRunner 回報回調（設定一次，所有施放共用）────────────────
    Runner.OnEntityDamage = [this](int32 EntityId, float Damage)
    {
        if (!CachedEnemyMgr) return;
        for (AEnemy* E : CachedEnemyMgr->GetEnemies())
        {
            if (!E || E->GetEntityId() != EntityId) continue;
            float HpBefore = E->GetHp();
            E->TakeDamageAmount(Damage);
            if (auto* GI = GetWorld()->GetGameInstance())
            if (auto* Sub = GI->GetSubsystem<UCombatStateSubsystem>())
            {
                Sub->OnPlayerDealtDamage(Damage);
                if (HpBefore > 0.f && E->GetHp() <= 0.f)
                    Sub->OnEnemyKilled();
            }
            return;
        }
    };

    Runner.OnEntityMove = [this](int32 EntityId, FGridPos NewPos)
    {
        if (!CachedEnemyMgr) return;
        for (AEnemy* E : CachedEnemyMgr->GetEnemies())
        {
            if (!E || E->GetEntityId() != EntityId) continue;
            E->GridPosition = NewPos;
            E->SetActorLocation(FVector((float)NewPos.X, (float)NewPos.Y, (float)NewPos.Z));
            return;
        }
    };

    Runner.OnInvokeTotem = [](FExecutionContext&, FName) {};          // M-8
    Runner.OnBuildComboContext = [](FName) -> TUniquePtr<FExecutionContext> { return nullptr; };
}

ASkillCreatorCharacter* USpellCaster::GetOwnerCharacter() const
{
    return Cast<ASkillCreatorCharacter>(GetOwner());
}

bool USpellCaster::TryCast(const FSpellArray& Spell, const TArray<FInstruction>& Code)
{
    if (CooldownRemaining > 0.f) return false;

    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    if (!Char || !Char->IsAlive()) return false;

    if (Char->CurrentMp < Spell.BaseMpCost) return false;
    Char->CurrentMp -= Spell.BaseMpCost;
    CooldownRemaining = FMath::Max(GlobalCooldown, Spell.CastDelay);

    if (auto* GI = GetWorld()->GetGameInstance())
    if (auto* Sub = GI->GetSubsystem<UCombatStateSubsystem>())
        Sub->OnSpellCast();

    // ── 投射物類型：生成 ASpellProjectile ────────────────────────────
    if (Spell.Container == EContainerType::Projectile)
    {
        ASpellProjectile* Proj = GetWorld()->SpawnActor<ASpellProjectile>();
        if (Proj)
        {
            FVector Fwd = Char->GetActorForwardVector();
            FIntVector Dir(
                FMath::Abs(Fwd.X) >= FMath::Abs(Fwd.Z) ? (Fwd.X > 0 ? 1 : -1) : 0,
                0,
                FMath::Abs(Fwd.Z) >  FMath::Abs(Fwd.X) ? (Fwd.Z > 0 ? 1 : -1) : 0
            );

                Proj->Init(Char->GetPosition(), Dir, CachedEnemyMgr, CachedVoxelWorld);

            float Dmg = Proj->BaseDamage;
            const ESkillElementType Elem = Spell.SpellElement;
            TWeakObjectPtr<USpellCaster> WeakThis(this);
            Proj->OnHitEnemy = [WeakThis, Dmg, Elem](AEnemy* Enemy, FGridPos HitPos)
            {
                if (!WeakThis.IsValid() || !Enemy) return;
                float HpBefore = Enemy->GetHp();
                Enemy->TakeDamageAmount(Dmg);
                UWorld* W = WeakThis->GetWorld();
                if (W)
                if (auto* GI = W->GetGameInstance())
                if (auto* Sub = GI->GetSubsystem<UCombatStateSubsystem>())
                {
                    Sub->OnPlayerDealtDamage(Dmg);
                    if (HpBefore > 0.f && Enemy->GetHp() <= 0.f)
                        Sub->OnEnemyKilled();
                }
                if (Elem != ESkillElementType::None)
                {
                    if (Enemy->AuraComp)
                        Enemy->AuraComp->ApplyImmediate(Elem, UElementalAuraComponent::DefaultAuraDuration, Enemy);
                    if (AVoxelWorldActor* VW = WeakThis->CachedVoxelWorld.Get())
                    if (FTileWorld3D* TW = VW->GetTileWorld())
                        TW->ApplyElementalImpact(HitPos.X, HitPos.Y, HitPos.Z, Elem);
                }
            };
        }
        return true;
    }

    // ── Contact 類型：前方掃描近戰命中 ───────────────────────────────
    if (Spell.Container == EContainerType::Contact)
    {
        ExecuteContactHit(Spell);
        return true;
    }

    // ── DirectCast：提交 VM 執行 Context ─────────────────────────────
    auto Ctx = MakeUnique<FExecutionContext>(Code);

    // 每次施放獨立的訊號集合
    auto Signals = MakeShared<TSet<FName>>();

    // 每次施放獨立的 Anchor 快照（用於 AnchorSnapshot / RollbackSnapshot 積木）
    struct FAnchorState
    {
        FIntVector              Min, Max;
        TArray<FTileCell>       Cells;
        bool                    bValid = false;
    };
    auto Anchor = MakeShared<FAnchorState>();

    FTileWorld3D* TW = CachedVoxelWorld ? CachedVoxelWorld->GetTileWorld() : nullptr;

    // EntityQuery：回傳半徑 Radius 格內的所有存活敵人
    Ctx->EntityQuery = [this, Char](float Radius) -> TArray<FEntityInfo>
    {
        TArray<FEntityInfo> Out;
        if (!CachedEnemyMgr) return Out;
        FGridPos CP = Char->GetPosition();
        for (AEnemy* E : CachedEnemyMgr->GetEnemies())
        {
            if (!E || !E->IsAlive()) continue;
            FGridPos EP = E->GetPosition();
            float Dist = FMath::Sqrt((float)((EP.X-CP.X)*(EP.X-CP.X) + (EP.Z-CP.Z)*(EP.Z-CP.Z)));
            if (Dist <= Radius)
                Out.Add({ E->GetEntityId(), EP, E->GetHp(), E->GetMaxHp() });
        }
        return Out;
    };

    // RaycastQuery：從 Start 格向 Dir 射線，最多 Dist 格
    Ctx->RaycastQuery = [TW](FGridPos Start, FVector Dir, float Dist) -> FRaycastResult
    {
        FRaycastResult Out;
        if (!TW) return Out;
        FVector StartF((float)Start.X, (float)Start.Y, (float)Start.Z);
        FRaycastResult3D R = TW->Raycast(StartF, Dir, Dist);
        Out.bHit  = R.bHit;
        Out.HitPos = FGridPos(R.HitCell.X, R.HitCell.Y, R.HitCell.Z);
        Out.MatId  = R.bHit ? (int32)TW->GetTile(R.HitCell.X, R.HitCell.Y, R.HitCell.Z) : 0;
        return Out;
    };

    // FocalPointQuery：角色正前方 5 格
    Ctx->FocalPointQuery = [Char]() -> FGridPos
    {
        FVector Fwd = Char->GetActorForwardVector();
        FGridPos CP = Char->GetPosition();
        return FGridPos(
            CP.X + FMath::RoundToInt(Fwd.X * 5.f),
            CP.Y + FMath::RoundToInt(Fwd.Y * 5.f),
            CP.Z + FMath::RoundToInt(Fwd.Z * 5.f)
        );
    };

    // PlayerStatsQuery：hp / mp / hpPct / mpPct
    Ctx->PlayerStatsQuery = [Char](FName Key) -> float
    {
        if (Key == "hp")    return Char->CurrentHp;
        if (Key == "mp")    return Char->CurrentMp;
        if (Key == "hpPct") return Char->Stats.MaxHpBase > 0.f ? Char->CurrentHp / Char->Stats.MaxHpBase : 0.f;
        if (Key == "mpPct") return Char->Stats.MaxMpBase > 0.f ? Char->CurrentMp / Char->Stats.MaxMpBase : 0.f;
        return 0.f;
    };

    // HasSignalFn / BroadcastFn：每次施放獨立訊號集合
    Ctx->HasSignalFn = [Signals](FName S) { return Signals->Contains(S); };
    Ctx->BroadcastFn = [Signals](FName S) { Signals->Add(S); };

    // BattleStatFn：戰鬥統計（UCombatStateSubsystem）
    Ctx->BattleStatFn = [this](FName Key) -> float
    {
        if (auto* GI = GetWorld()->GetGameInstance())
        if (auto* Sub = GI->GetSubsystem<UCombatStateSubsystem>())
        {
            if (Key == "castCount")   return (float)Sub->CastCount;
            if (Key == "damageDealt") return Sub->DamageDealt;
            if (Key == "killCount")   return (float)Sub->KillCount;
            if (Key == "battleId")    return (float)Sub->BattleId;
        }
        return 0.f;
    };

    // TookDamageThisTickFn
    Ctx->TookDamageThisTickFn = [this]() -> bool
    {
        if (auto* GI = GetWorld()->GetGameInstance())
        if (auto* Sub = GI->GetSubsystem<UCombatStateSubsystem>())
            return Sub->bTookDamageThisFrame;
        return false;
    };

    // AnchorSnapshot：快照角色周圍 Radius 格的 tile 狀態
    Ctx->AnchorAction = [TW, Char, Anchor](int32 Radius)
    {
        if (!TW) return;
        FGridPos CP = Char->GetPosition();
        Anchor->Min = FIntVector(CP.X - Radius, CP.Y - Radius, CP.Z - Radius);
        Anchor->Max = FIntVector(CP.X + Radius, CP.Y + Radius, CP.Z + Radius);
        Anchor->Cells  = TW->SnapshotRegion(Anchor->Min, Anchor->Max);
        Anchor->bValid = true;
    };

    // RollbackSnapshot：恢復快照
    Ctx->RollbackAction = [TW, Anchor]()
    {
        if (!TW || !Anchor->bValid) return;
        TW->RestoreRegion(Anchor->Min, Anchor->Max, Anchor->Cells);
        Anchor->bValid = false;
    };

    // SetActivationModeFn：M-9 實作
    Ctx->SetActivationModeFn = [](int32) {};
    // RegisterFilterFn：轉送到 ActionBus（DeltaTime 計時，pause-aware）
    Ctx->RegisterFilterFn = [Char](FName FilterType, FName Mode, bool bOneShot,
                                   float Threshold, float CapValue)
    {
        if (Char) Char->ActionBus.RegisterFilter(FilterType, Mode, bOneShot, Threshold, CapValue);
    };

    Runner.Submit(MoveTemp(Ctx));
    return true;
}

void USpellCaster::SwitchSlot(int32 Idx)
{
    SpellGroups.GetActiveLoadout().ActiveIndex =
        FMath::Clamp(Idx, 0, FSpellLoadout::MaxSlots - 1);
}

void USpellCaster::CycleSlot(int32 Delta)
{
    FSpellLoadout& L = SpellGroups.GetActiveLoadout();
    const int32 N    = FSpellLoadout::MaxSlots;
    L.ActiveIndex    = ((L.ActiveIndex + Delta) % N + N) % N;
}

void USpellCaster::TryCastSlot(int32 SlotIndex)
{
    FSpellLoadout& L = SpellGroups.GetActiveLoadout();
    if (!L.Slots.IsValidIndex(SlotIndex)) return;
    SwitchSlot(SlotIndex);
    // M-9: full SpellCompiler pipeline here. For M-5: pass empty code.
    TryCast(L.Slots[SlotIndex], {});
}

void USpellCaster::ExecuteContactHit(const FSpellArray& Spell)
{
    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    if (!Char || !CachedEnemyMgr) return;

    const FVector  Fwd    = Char->GetActorForwardVector();
    const FGridPos Origin = Char->GetPosition();
    constexpr int32 MeleeRange = 3;

    // 前方 MeleeRange 格 3D 掃描，找第一個存活敵人
    AEnemy*   Target = nullptr;
    FGridPos  HitPos;
    for (int32 Step = 1; Step <= MeleeRange && !Target; ++Step)
    {
        FGridPos Check(
            Origin.X + FMath::RoundToInt(Fwd.X * (float)Step),
            Origin.Y + FMath::RoundToInt(Fwd.Y * (float)Step),
            Origin.Z + FMath::RoundToInt(Fwd.Z * (float)Step)
        );
        for (AEnemy* E : CachedEnemyMgr->GetEnemies())
        {
            if (!E || !E->IsAlive()) continue;
            const FGridPos EP = E->GetPosition();
            if (EP.X == Check.X && EP.Y == Check.Y && EP.Z == Check.Z)
            {
                Target = E;
                HitPos = EP;
                break;
            }
        }
    }

    if (!Target) return;

    // 元素 Aura 施加（無冷卻命中）
    if (Spell.SpellElement != ESkillElementType::None && Target->AuraComp)
        Target->AuraComp->ApplyImmediate(Spell.SpellElement, UElementalAuraComponent::DefaultAuraDuration, Target);

    // 傷害（基礎值；M-9 接 SpellRunner 後改由 VM 計算）
    constexpr float BaseDamage = 10.f;
    const float HpBefore = Target->GetHp();
    Target->TakeDamageAmount(BaseDamage);

    if (auto* GI = GetWorld()->GetGameInstance())
    if (auto* Sub = GI->GetSubsystem<UCombatStateSubsystem>())
    {
        Sub->OnPlayerDealtDamage(BaseDamage);
        if (HpBefore > 0.f && Target->GetHp() <= 0.f)
            Sub->OnEnemyKilled();
    }

    // SpawnEffect：命中 tile 觸發元素 CA 反應
    if (Spell.SpellElement != ESkillElementType::None && CachedVoxelWorld)
    {
        if (FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld())
            TW->ApplyElementalImpact(HitPos.X, HitPos.Y, HitPos.Z, Spell.SpellElement);
    }
}

void USpellCaster::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (CooldownRemaining > 0.f)
        CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaTime);

    Runner.Tick(DeltaTime);
}
