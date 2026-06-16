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

    Runner.OnInvokeTotem = [](FExecutionContext&, FName) {};  // fallback; InvokeTotemFn on Ctx takes priority
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

    // W-6: 優先從技能使用的法力類型插槽扣除；找不到對應 key 則降格至 slot[0]
    FManaSlot* Source = nullptr;
    for (const FName& Key : Spell.GetUsedManaTypes())
    {
        Source = Char->GetManaSlot(Key);
        if (Source) break;
    }
    if (!Source && Char->ActiveManaSlots.Num() > 0)
        Source = &Char->ActiveManaSlots[0];

    float Available = Source ? Source->Current : Char->CurrentMp;
    if (Available < Spell.BaseMpCost) return false;

    if (Source)
        Source->Consume(Spell.BaseMpCost);
    else
        Char->CurrentMp -= Spell.BaseMpCost;

    if (Char->ActiveManaSlots.Num() > 0)
        Char->CurrentMp = Char->ActiveManaSlots[0].Current;
    CooldownRemaining = FMath::Max(GlobalCooldown, Spell.CastDelay);
    ApplyGlobalEngravings(Spell);

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
            Proj->Init(Char->GetPosition(), Fwd, CachedEnemyMgr, CachedVoxelWorld);

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

    // InvokeTotemFn：每次施放按此技能整構的 slots 建立 slot 查表，捕捉到 lambda
    {
        auto SlotLookup = MakeShared<TMap<FName, FSpellSlot>>();
        for (int32 i = 0; i < Spell.Slots.Num(); ++i)
        {
            const FSpellSlot& S = Spell.Slots[i];
            if (S.IsEmpty()) continue;
            FName Key = S.Name.IsNone()
                ? FName(*FString::Printf(TEXT("slot_%d"), i))
                : S.Name;
            SlotLookup->Add(Key, S);
        }
        TWeakObjectPtr<USpellCaster>    WeakThis(this);
        TWeakObjectPtr<AVoxelWorldActor> WeakVW(CachedVoxelWorld.Get());
        Ctx->InvokeTotemFn = [WeakThis, SlotLookup, WeakVW](FExecutionContext& C, FName TotemName)
        {
            USpellCaster* Caster = WeakThis.Get();
            if (!Caster) return;
            const FSpellSlot* Slot = SlotLookup->Find(TotemName);
            if (!Slot) { C.DoneTotems.Add(TotemName); return; }
            bool bAtHit = C.FixedOrigin.IsSet();
            Caster->ResolveTotem(TotemName, *Slot, C, bAtHit, WeakVW.Get());
        };
    }

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

// ─────────────────────────────────────────────────────────────────────
//  技能施放核心 — 對應 Godot SpellCaster.cs §ReadMods / DispatchAction 等
// ─────────────────────────────────────────────────────────────────────

static constexpr int32 FanRange   = 5;
static constexpr int32 BeamRange  = 12;
static constexpr int32 DashSteps  = 10;
static constexpr int32 DodgeSteps = 5;
static constexpr float NearbyR    = 3.f;

USpellCaster::FSpellMods USpellCaster::ReadMods(const FSpellSlot& Slot)
{
    FSpellMods M;
    for (const FEngraveData& E : Slot.LocalEngravings)
    {
        const FName& Id = E.EngraveId;
        if      (Id == "white_dmg")     M.DmgBonus  = E.CalculateEffect();
        else if (Id == "blue_multi")    M.Multi     = FMath::Max(1, (int32)E.CalculateEffect());
        else if (Id == "elem_fire")     M.bFire     = true;
        else if (Id == "elem_water")    M.bWater    = true;
        else if (Id == "elem_ice")      M.bIce      = true;
        else if (Id == "elem_thunder")  M.bThunder  = true;
        else if (Id == "orange_push")   M.PushDist  = E.CalculateEffect();
        else if (Id == "orange_pull")   M.PullDist  = E.CalculateEffect();
        else if (Id == "orange_slow")   M.SlowDur   = E.CalculateEffect();
        else if (Id == "orange_freeze") M.FreezeDur = E.CalculateEffect();
        else if (Id == "red_stun")      M.StunDur   = E.CalculateEffect();
        else if (Id == "yellow_hp")     M.HpCost    = E.CalculateEffect();
    }
    return M;
}

void USpellCaster::SpawnEffectTiles(AVoxelWorldActor* VW, FName EffectType,
                                     FGridPos Pos, int32 Radius)
{
    if (!VW) return;
    FTileWorld3D* TW = VW->GetTileWorld();
    if (!TW) return;
    const bool bFire = (EffectType == "fire");
    for (int32 dx = -Radius; dx <= Radius; ++dx)
    for (int32 dy = -Radius; dy <= Radius; ++dy)
    for (int32 dz = -Radius; dz <= Radius; ++dz)
    {
        if (dx*dx + dy*dy + dz*dz > Radius*Radius) continue;
        int32 tx = Pos.X+dx, ty = Pos.Y+dy, tz = Pos.Z+dz;
        if (!TW->InBounds(tx, ty, tz)) continue;
        if (TW->GetTile(tx, ty, tz) != EMaterialType::Air) continue;
        if (bFire) TW->SetFire(tx, ty, tz);
        else       TW->SetTile(tx, ty, tz, EMaterialType::Water);
    }
}

void USpellCaster::ApplyGlobalEngravings(const FSpellArray& Spell)
{
    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    if (!Char) return;
    for (const FEngraveData& E : Spell.GlobalEngravings)
    {
        if (E.EngraveId == "green_death_replace")
            Char->ActionBus.RegisterFilter("DeathGuard", "Once", true, 0.f, 0.f);
        else if (E.EngraveId == "green_invincible")
            Char->ActionBus.RegisterFilter("DamageShield", "Cancel", false,
                                            FMath::Max(0.f, E.CalculateEffect()), 0.f);
    }
}

void USpellCaster::ApplyModsToNearbyEnemies(const FSpellMods& Mods, FGridPos Origin,
                                              AVoxelWorldActor* VW)
{
    if (!CachedEnemyMgr) return;
    for (AEnemy* Enemy : CachedEnemyMgr->GetEnemies())
    {
        if (!Enemy || !Enemy->IsAlive()) continue;
        FGridPos EP = Enemy->GetPosition();
        float Dist = FMath::Sqrt((float)((EP.X-Origin.X)*(EP.X-Origin.X)
                                        +(EP.Z-Origin.Z)*(EP.Z-Origin.Z)));
        if (Dist > NearbyR) continue;

        if (Mods.PushDist > 0.f)
        {
            FVector Away = (FVector(EP.X,0,EP.Z)-FVector(Origin.X,0,Origin.Z)).GetSafeNormal();
            Enemy->SetActorLocation(Enemy->GetActorLocation()
                + Away * Mods.PushDist * WorldScale::TileSizeCm,
                true, nullptr, ETeleportType::None);
        }
        if (Mods.PullDist > 0.f)
        {
            FVector To = (FVector(Origin.X,0,Origin.Z)-FVector(EP.X,0,EP.Z)).GetSafeNormal();
            Enemy->SetActorLocation(Enemy->GetActorLocation()
                + To * Mods.PullDist * WorldScale::TileSizeCm,
                true, nullptr, ETeleportType::None);
        }

        auto ApplyElem = [Enemy, EP, VW](ESkillElementType Elem)
        {
            if (Enemy->AuraComp)
                Enemy->AuraComp->ApplyImmediate(Elem,
                    UElementalAuraComponent::DefaultAuraDuration, Enemy);
            if (VW) if (FTileWorld3D* TW = VW->GetTileWorld())
                TW->ApplyElementalImpact(EP.X, EP.Y, EP.Z, Elem);
        };

        if (Mods.bFire)    ApplyElem(ESkillElementType::Fire);
        if (Mods.bWater)   ApplyElem(ESkillElementType::Water);
        if (Mods.bIce)     ApplyElem(ESkillElementType::Ice);
        if (Mods.bThunder) ApplyElem(ESkillElementType::Thunder);
        if (Mods.HpCost > 0.f) Enemy->TakeDamageAmount(Mods.HpCost);
        // Slow/Freeze/Stun → W-5 status effects
    }
}

void USpellCaster::ResolveTotem(FName TotemName, const FSpellSlot& Slot,
                                  FExecutionContext& Ctx, bool bAtHitPoint,
                                  AVoxelWorldActor* VW)
{
    if (Ctx.DoneTotems.Contains(TotemName)) return;

    FGridPos Origin;
    if (Ctx.CurrentIterEntity.IsSet())
        Origin = Ctx.CurrentIterEntity.GetValue().Position;
    else if (Ctx.FixedOrigin.IsSet())
        Origin = Ctx.FixedOrigin.GetValue();
    else if (ASkillCreatorCharacter* Char = GetOwnerCharacter())
        Origin = Char->GetPosition();

    for (const FEngraveData& E : Slot.LocalEngravings)
    {
        if (E.Category == EEngraveCategory::Action &&
            E.Trigger  == EEngraveTrigger::OnCast)
        {
            DispatchAction(E.EngraveId.ToString(), Slot, Origin, bAtHitPoint, VW);
            break;
        }
    }

    Ctx.HitTotems.Add(TotemName);
    Ctx.DoneTotems.Add(TotemName);
}

void USpellCaster::DispatchAction(const FString& ActionId, const FSpellSlot& Slot,
                                   FGridPos Origin, bool bAtHitPoint, AVoxelWorldActor* VW)
{
    if (ActionId == "act_area_fan"    || ActionId == "act_area_around" ||
        ActionId == "act_area_distant"|| ActionId == "act_area_beam")
        ExecuteArea(ActionId, Slot, Origin, bAtHitPoint, VW);
    else if (ActionId == "act_technique_sword" ||
             ActionId == "act_technique_punch" ||
             ActionId == "act_technique_shield")
        ExecuteTechnique(Slot, Origin, bAtHitPoint, VW);
    else if (ActionId == "act_fire_projectile")
        ExecuteProjectileTotem(Slot, Origin, VW);
    else if (ActionId == "act_morph_apply")
        ExecuteMorph(Slot, Origin, VW);
    else if (ActionId == "act_dash"    || ActionId == "act_teleport" ||
             ActionId == "act_dodge"   || ActionId == "act_portal")
        ExecuteDisplacement(ActionId, Slot, VW);
    else if (ActionId == "act_summon_entity")
        ExecuteSummon(Slot, Origin, VW);
    else if (ActionId == "act_domain_activate")
        ExecuteDomain(Slot, Origin, VW);
    // act_passive_tick → no-op on OnCast
}

void USpellCaster::ExecuteArea(const FString& Shape, const FSpellSlot& Slot,
                                FGridPos Origin, bool bFromHitPoint, AVoxelWorldActor* VW)
{
    FTileWorld3D* TW = VW ? VW->GetTileWorld() : nullptr;
    const FSpellMods M = ReadMods(Slot);

    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    if (!bFromHitPoint && Char)
        Origin.Y += WorldScale::PlayerH / 2;

    const int32 Grain = WorldScale::GrainCurrent;
    const int32 BaseR = (2 + (int32)(M.DmgBonus * 3.f)) * Grain;

    FVector Fwd(1.f, 0.f, 0.f);
    if (Char) Fwd = Char->GetActorForwardVector();
    const int32 FX = FMath::Abs(Fwd.X) >= FMath::Abs(Fwd.Z) ? (Fwd.X > 0 ? 1 : -1) : 0;
    const int32 FZ = FMath::Abs(Fwd.Z) >  FMath::Abs(Fwd.X) ? (Fwd.Z > 0 ? 1 : -1) : 0;
    const int32 PX = -FZ, PZ = FX;

    if (TW)
    {
        if (Shape == "act_area_around")
        {
            TW->Explode(Origin.X, Origin.Y, Origin.Z, BaseR + 1);
        }
        else if (Shape == "act_area_fan")
        {
            for (int32 d = 0; d < FanRange * Grain; ++d)
            for (int32 w = -d; w <= d; ++w)
            {
                int32 tx = Origin.X + FX*d + PX*w;
                int32 tz = Origin.Z + FZ*d + PZ*w;
                if (!TW->InBounds(tx, Origin.Y, tz)) continue;
                if (TW->GetTile(tx, Origin.Y, tz) != EMaterialType::Air) break;
                TW->Explode(tx, Origin.Y, tz, 1);
            }
        }
        else if (Shape == "act_area_distant")
        {
            int32 dist = (6 + BaseR) * Grain;
            TW->Explode(Origin.X + FX*dist, Origin.Y, Origin.Z + FZ*dist, BaseR + 1);
        }
        else if (Shape == "act_area_beam")
        {
            const FName BeamType = M.bFire ? FName("fire") : FName("water");
            for (int32 d = 1; d <= BeamRange * Grain; ++d)
            {
                int32 tx = Origin.X + FX*d, tz = Origin.Z + FZ*d;
                if (!TW->InBounds(tx, Origin.Y, tz)) break;
                if (TW->GetTile(tx, Origin.Y, tz) != EMaterialType::Air) break;
                SpawnEffectTiles(VW, BeamType, FGridPos(tx, Origin.Y, tz), 1);
            }
        }
    }

    ApplyModsToNearbyEnemies(M, Origin, VW);
}

void USpellCaster::ExecuteTechnique(const FSpellSlot& Slot, FGridPos Origin,
                                     bool bAtHitPoint, AVoxelWorldActor* VW)
{
    FTileWorld3D* TW = VW ? VW->GetTileWorld() : nullptr;
    if (!TW) return;
    const FSpellMods M = ReadMods(Slot);

    FVector Fwd(1.f, 0.f, 0.f);
    if (ASkillCreatorCharacter* Char = GetOwnerCharacter()) Fwd = Char->GetActorForwardVector();
    const int32 FX = FMath::Abs(Fwd.X) >= FMath::Abs(Fwd.Z) ? (Fwd.X > 0 ? 1 : -1) : 0;
    const int32 FZ = FMath::Abs(Fwd.Z) >  FMath::Abs(Fwd.X) ? (Fwd.Z > 0 ? 1 : -1) : 0;

    const FName TotemId = Slot.TotemId;
    if (TotemId == "technique_sword" || TotemId == "technique_slash")
    {
        for (int32 i = 1; i <= 3; ++i)
            TW->Explode(Origin.X + FX*i, Origin.Y, Origin.Z + FZ*i, 2);
    }
    else if (TotemId == "technique_punch")
    {
        TW->Explode(Origin.X + FX, Origin.Y, Origin.Z + FZ, 1);
    }
    else if (TotemId == "technique_shield")
    {
        TW->Explode(Origin.X, Origin.Y, Origin.Z, 3);
    }
    else
    {
        TW->Explode(Origin.X + FX, Origin.Y, Origin.Z + FZ, 2);
    }

    ApplyModsToNearbyEnemies(M, Origin, VW);
}

void USpellCaster::ExecuteProjectileTotem(const FSpellSlot& Slot, FGridPos Origin,
                                           AVoxelWorldActor* VW)
{
    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    if (!Char) return;

    ASpellProjectile* Proj = GetWorld()->SpawnActor<ASpellProjectile>();
    if (!Proj) return;

    FVector Fwd = Char->GetActorForwardVector();
    Proj->Init(Origin, Fwd, CachedEnemyMgr, VW ? VW : CachedVoxelWorld.Get());

    const FSpellMods M = ReadMods(Slot);
    float Dmg = Proj->BaseDamage * (1.f + M.DmgBonus);
    TWeakObjectPtr<AVoxelWorldActor> WeakVW2(VW);
    Proj->OnHitEnemy = [Dmg, M, WeakVW2](AEnemy* Enemy, FGridPos HitPos)
    {
        if (!Enemy) return;
        Enemy->TakeDamageAmount(Dmg);
        AVoxelWorldActor* VW2 = WeakVW2.Get();
        if (M.bFire)    USpellCaster::SpawnEffectTiles(VW2, "fire",  HitPos, 1);
        if (M.bWater)   USpellCaster::SpawnEffectTiles(VW2, "water", HitPos, 1);
        if (Enemy->AuraComp)
        {
            if (M.bFire)    Enemy->AuraComp->ApplyImmediate(ESkillElementType::Fire,
                                UElementalAuraComponent::DefaultAuraDuration, Enemy);
            if (M.bWater)   Enemy->AuraComp->ApplyImmediate(ESkillElementType::Water,
                                UElementalAuraComponent::DefaultAuraDuration, Enemy);
            if (M.bIce)     Enemy->AuraComp->ApplyImmediate(ESkillElementType::Ice,
                                UElementalAuraComponent::DefaultAuraDuration, Enemy);
            if (M.bThunder) Enemy->AuraComp->ApplyImmediate(ESkillElementType::Thunder,
                                UElementalAuraComponent::DefaultAuraDuration, Enemy);
        }
    };
}

void USpellCaster::ExecuteMorph(const FSpellSlot& Slot, FGridPos Origin, AVoxelWorldActor* VW)
{
    FTileWorld3D* TW = VW ? VW->GetTileWorld() : nullptr;
    if (!TW) return;
    const FSpellMods M = ReadMods(Slot);
    const int32 Radius = FMath::Max(1, 1 + (int32)M.DmgBonus);
    for (int32 dx = -Radius; dx <= Radius; ++dx)
    for (int32 dy = -Radius; dy <= Radius; ++dy)
    for (int32 dz = -Radius; dz <= Radius; ++dz)
    {
        if (dx*dx + dy*dy + dz*dz > Radius*Radius) continue;
        int32 tx = Origin.X+dx, ty = Origin.Y+dy, tz = Origin.Z+dz;
        if (!TW->InBounds(tx, ty, tz)) continue;
        if (TW->GetTile(tx, ty, tz) == EMaterialType::Air)
            TW->SetTile(tx, ty, tz, EMaterialType::Stone);
    }
}

void USpellCaster::ExecuteDisplacement(const FString& ActionId, const FSpellSlot& Slot,
                                        AVoxelWorldActor* VW)
{
    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    if (!Char) return;
    FTileWorld3D* TW = VW ? VW->GetTileWorld() : nullptr;

    FVector Fwd = Char->GetActorForwardVector();
    const int32 FX = FMath::Abs(Fwd.X) >= FMath::Abs(Fwd.Z) ? (Fwd.X > 0 ? 1 : -1) : 0;
    const int32 FZ = FMath::Abs(Fwd.Z) >  FMath::Abs(Fwd.X) ? (Fwd.Z > 0 ? 1 : -1) : 0;
    FGridPos Pos = Char->GetPosition();

    if (ActionId == "act_dash" || ActionId == "act_dodge")
    {
        const int32 MaxSteps = (ActionId == "act_dodge") ? DodgeSteps : DashSteps;
        const int32 DX = (ActionId == "act_dodge") ? -FX : FX;
        const int32 DZ = (ActionId == "act_dodge") ? -FZ : FZ;
        FGridPos Final = Pos;
        for (int32 i = 1; i <= MaxSteps; ++i)
        {
            FGridPos Next(Pos.X + DX*i, Pos.Y, Pos.Z + DZ*i);
            if (TW && TW->GetTile(Next.X, Next.Y, Next.Z) != EMaterialType::Air) break;
            Final = Next;
        }
        if (Final.X != Pos.X || Final.Z != Pos.Z)
            Char->SetActorLocation(
                FVector((float)Final.X, Char->GetActorLocation().Y, (float)Final.Z),
                false, nullptr, ETeleportType::TeleportPhysics);
    }
    else // act_teleport / act_portal
    {
        const int32 MaxSteps = 15 * WorldScale::GrainCurrent;
        FGridPos Target = Pos;
        for (int32 i = 1; i <= MaxSteps; ++i)
        {
            FGridPos Next(Pos.X + FX*i, Pos.Y, Pos.Z + FZ*i);
            if (TW && TW->GetTile(Next.X, Next.Y, Next.Z) != EMaterialType::Air) break;
            Target = Next;
        }
        Char->TeleportTo(
            FVector((float)Target.X, Char->GetActorLocation().Y, (float)Target.Z),
            Char->GetActorRotation());
    }
}

void USpellCaster::ExecuteSummon(const FSpellSlot& Slot, FGridPos Origin, AVoxelWorldActor* VW)
{
    // W-11: 召喚實體工廠系統完成後實作
    UE_LOG(LogTemp, Warning, TEXT("[SpellCaster] ExecuteSummon stub: %s"), *Slot.TotemId.ToString());
}

void USpellCaster::ExecuteDomain(const FSpellSlot& Slot, FGridPos Origin, AVoxelWorldActor* VW)
{
    FTileWorld3D* TW = VW ? VW->GetTileWorld() : nullptr;
    if (!TW) return;
    const FSpellMods M = ReadMods(Slot);
    const int32 DomainR = (4 + (int32)(M.DmgBonus * 4.f)) * WorldScale::GrainCurrent;

    TW->Explode(Origin.X, Origin.Y, Origin.Z, DomainR);
    if (M.bFire)  SpawnEffectTiles(VW, "fire",  Origin, DomainR);
    if (M.bWater) SpawnEffectTiles(VW, "water", Origin, DomainR);
    ApplyModsToNearbyEnemies(M, Origin, VW);
}

// ─────────────────────────────────────────────────────────────────────

void USpellCaster::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (CooldownRemaining > 0.f)
        CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaTime);

    Runner.Tick(DeltaTime);
}
