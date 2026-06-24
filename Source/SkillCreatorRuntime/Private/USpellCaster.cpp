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
#include "AbilityPointCalculator.h"
#include "SpellCompiler.h"
#include "GameFramework/PlayerController.h"

// Anchor 快照狀態（TryCast + BuildContext 共用）
struct FAnchorState
{
    FIntVector        Min, Max;
    TArray<FTileCell> Cells;
    bool              bValid = false;
};

// ── EventBus 全域訊號集合（對應 Godot EventBus.cs:6-24 的 static HashSet）────────
// 2026-06-20 Round3 C-7 修正：原本每次 BuildContext 都建立全新 per-cast 局部 TSet，
// 導致同一玩家的技能A Broadcast 訊號物理上無法被技能B 的 OnReceive 收到。改為檔案級
// static 集合，所有 USpellCaster 實例共享，並在每個新的引擎 Tick 開頭清空一次
// （對應 Godot Main._Process 開頭呼叫 EventBus.ClearFrame()）。用 GFrameCounter 而非
// 「每次 TickComponent 呼叫就清空」，避免同一幀內多個 SpellCaster 實例互相清掉彼此剛
// Broadcast 的訊號。
static TSet<FName> GEventBusSignals;
static uint64      GEventBusLastClearedFrame = (uint64)-1;

static void EventBus_ClearFrameIfNeeded()
{
    if (GFrameCounter != GEventBusLastClearedFrame)
    {
        GEventBusSignals.Reset();
        GEventBusLastClearedFrame = GFrameCounter;
    }
}

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
                Sub->OnHit.Broadcast(E->GridPosition, Damage, false);
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

    // C-5 修正：PruneAfter 清除時退還已扣除 MP（對應 Godot SpellRunner.cs:118）
    Runner.OnMpRefund = [this](float MpAmount)
    {
        ASkillCreatorCharacter* RefChar = GetOwnerCharacter();
        if (!RefChar) return;
        if (RefChar->ActiveManaSlots.Num() > 0)
        {
            FManaSlot& Slot = RefChar->ActiveManaSlots[0];
            Slot.Current = FMath::Min(Slot.Max, Slot.Current + MpAmount);
            RefChar->CurrentMp = Slot.Current;
        }
        else
            RefChar->CurrentMp = FMath::Min(100.f, RefChar->CurrentMp + MpAmount);
    };

    Runner.OnInvokeTotem = [](FExecutionContext&, FName) {};  // fallback; InvokeTotemFn on Ctx takes priority
    // InvokeSpell 連段：從玩家 SpellGroup 查找對應 Spell 並編譯新 Context
    Runner.OnBuildComboContext = [this](FName SpellName) -> TUniquePtr<FExecutionContext>
    {
        FSpellLoadout& L = SpellGroups.GetActiveLoadout();
        for (const FSpellArray& Spell : L.Slots)
        {
            if (FName(*Spell.Name) != SpellName) continue;
            if (!Spell.Blocks.IsValid() || Spell.Blocks->IsEmpty()) return nullptr;

            TMap<FName, FName> TotemSlotMap;
            for (int32 i = 0; i < Spell.Slots.Num(); ++i)
            {
                const FSpellSlot& S = Spell.Slots[i];
                if (S.IsEmpty()) continue;
                FName Key = S.Name.IsNone()
                    ? FName(*FString::Printf(TEXT("slot_%d"), i))
                    : S.Name;
                if (!S.TotemId.IsNone())
                    TotemSlotMap.Add(S.TotemId, Key);
            }
            TArray<FInstruction> Code = FSpellCompiler::Compile(*Spell.Blocks, TotemSlotMap);
            if (Code.IsEmpty()) return nullptr;
            return BuildContext(Spell, Code);
        }
        return nullptr;
    };
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

    // 2026-06-19 稽核發現：ExceedsLevelCap() 寫對了但從未在施法路徑被呼叫，玩家可無視
    // 能力點上限亂施法。對應 Godot SpellCaster.TryCast() 的等級上限拒放檢查。
    if (FAbilityPointCalculator::ExceedsLevelCap(Spell, Char->Level)) return false;

    // B-2 修正：依 ManaTypeKey 按比例分配 MP，先全部驗證再原子扣除
    // 對應 Godot SpellCaster.cs:251-275 TryConsumeMp（bound.Count=0 回退 slot[0]，否則均分）
    const float TotalMpCost = FAbilityPointCalculator::CalculateMpCost(Spell);
    const TMap<FName, float> CostByType = FAbilityPointCalculator::CalculateSlotCostByType(Spell);

    if (CostByType.IsEmpty())
    {
        // 無 ManaTypeKey 插槽：回退到 ActiveManaSlots[0]（對應 Godot gui_dao 預設槽）
        FManaSlot* Def = Char->ActiveManaSlots.Num() > 0 ? &Char->ActiveManaSlots[0] : nullptr;
        if (!Def || Def->Current < TotalMpCost) return false;
        Def->Current -= TotalMpCost;
    }
    else
    {
        // 先全部驗證（原子性），再統一扣除
        for (const auto& [Key, Cost] : CostByType)
        {
            const FManaSlot* MS = Char->GetManaSlot(Key);
            if (!MS || MS->Current < Cost) return false;
        }
        for (const auto& [Key, Cost] : CostByType)
            Char->GetManaSlot(Key)->Current -= Cost;
    }
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
            // B-3 修正：取攝影機前向作瞄準方向（對應 Godot SpellCaster.cs:56-70 滑鼠格位差正規化）
        // 並從身體邊緣起點發射（halfW+1 格偏移，對應 Godot startX=pos.X+sdx*(halfW+1)）
        FVector Fwd = Char->GetActorForwardVector();
        if (APlayerController* PC = Cast<APlayerController>(Char->GetController()))
        {
            FVector CLoc; FRotator CRot;
            PC->GetPlayerViewPoint(CLoc, CRot);
            Fwd = CRot.Vector();
        }
        const int32 HalfW = WorldScale::GrainCurrent / 2;  // = 8 tiles
        const int32 HalfH = WorldScale::GrainCurrent;       // = 16 tiles (PlayerH/2)
        const FGridPos CPos = Char->GetPosition();
        const int32 SX = (Fwd.X > 0.01f) ? 1 : ((Fwd.X < -0.01f) ? -1 : 0);
        const int32 SZ = (Fwd.Z > 0.01f) ? 1 : ((Fwd.Z < -0.01f) ? -1 : 0);
        FGridPos ProjStart(CPos.X + SX * (HalfW + 1), CPos.Y + HalfH, CPos.Z + SZ * (HalfW + 1));
            Proj->Init(ProjStart, Fwd, CachedEnemyMgr, CachedVoxelWorld);

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
                    Sub->OnHit.Broadcast(HitPos, Dmg, false);
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
        ExecuteContactHit(Spell, Code);
        return true;
    }

    // ── DirectCast / SummonMinion / SummonTurret / SummonGuardian：────
    // 提交 VM 執行 Context。召喚容器目前等同 DirectCast（對應 Godot SpellCaster.cs:86-112
    // ExecuteSummonContainer 已知 TODO-STUB：容器類型存在但行為等同直接施放，召喚物 AI
    // 本體延遲至 W-11；顯式列出三個 case 避免被誤判為遺漏，詳見 docs/audit-round3-2026-06-19.md B-5）
    auto Ctx = BuildContext(Spell, Code);
    if (!Ctx) return false;
    Runner.Submit(MoveTemp(Ctx), TotalMpCost);  // C-5：記錄 MpCost 供 PruneAfter 退還
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
    const FSpellArray& Spell = L.Slots[SlotIndex];

    TArray<FInstruction> Code;
    if (Spell.Blocks.IsValid() && !Spell.Blocks->IsEmpty())
    {
        TMap<FName, FName> TotemSlotMap;
        for (int32 i = 0; i < Spell.Slots.Num(); ++i)
        {
            const FSpellSlot& S = Spell.Slots[i];
            if (S.IsEmpty()) continue;
            FName Key = S.Name.IsNone()
                ? FName(*FString::Printf(TEXT("slot_%d"), i))
                : S.Name;
            if (!S.TotemId.IsNone())
                TotemSlotMap.Add(S.TotemId, Key);
        }
        Code = FSpellCompiler::Compile(*Spell.Blocks, TotemSlotMap);
    }
    TryCast(Spell, Code);
}

void USpellCaster::ExecuteContactHit(const FSpellArray& Spell, const TArray<FInstruction>& Code)
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

    if (Target)
    {
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
    else
    {
        // 未命中：在正前方 2 格執行 VM 效果（AoE 仍可擊中範圍內目標）
        // 對應 Godot SpellCaster.cs:150-154：meleePt = pos + fx*2，runner.Submit 仍會觸發
        HitPos = FGridPos(
            Origin.X + FMath::RoundToInt(Fwd.X * 2.f),
            Origin.Y + FMath::RoundToInt(Fwd.Y * 2.f),
            Origin.Z + FMath::RoundToInt(Fwd.Z * 2.f)
        );
    }

    // B-4 修正：提交 VM 在命中點執行（對應 Godot SpellCaster.cs:134-136 runner.Submit fixedOrigin）
    if (Code.Num() > 0)
    {
        if (auto Ctx = BuildContext(Spell, Code))
        {
            Ctx->FixedOrigin = HitPos;
            Runner.Submit(MoveTemp(Ctx));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────
//  技能施放核心 — 對應 Godot SpellCaster.cs §ReadMods / DispatchAction 等
// ─────────────────────────────────────────────────────────────────────

static constexpr int32 DashSteps  = 10;
static constexpr int32 DodgeSteps = 5;

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
        else if (Id == "yellow_hp_cost") M.HpCost   = E.CalculateEffect();
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
    // HP 代價是施法者自損換效果（對應 Godot SpellCaster.cs:445 player.TakeDamage(m.HpCost)，
    // "yellow_hp_cost" 刻印的 DisplayName 即「HP代價」），不是對敵人造成傷害。
    // 2026-06-19 稽核發現先前語意寫反：對敵人造成免費額外傷害，現修正套用對象為施法者。
    if (Mods.HpCost > 0.f)
        if (ASkillCreatorCharacter* Char = GetOwnerCharacter())
            Char->TakeDirectDamage(Mods.HpCost);

    // S-7: 半徑依 DmgBonus 刻印動態計算（對應 Godot SpellCaster.cs:684 4+dmgBonus*3）
    const float NearbyR = 4.f + Mods.DmgBonus * 3.f;

    if (!CachedEnemyMgr) return;
    for (AEnemy* Enemy : CachedEnemyMgr->GetEnemies())
    {
        if (!Enemy || !Enemy->IsAlive()) continue;
        FGridPos EP = Enemy->GetPosition();
        float Dist = FMath::Sqrt((float)((EP.X-Origin.X)*(EP.X-Origin.X)
                                        +(EP.Z-Origin.Z)*(EP.Z-Origin.Z)));
        if (Dist > NearbyR) continue;

        // B-17 修正：逐格碰撞檢測（對應 Godot SpellCaster.cs:695-715）
        // 原版整體 SetActorLocation 無視牆壁，現改為每格確認為空氣才移動
        FTileWorld3D* TW_Mods = VW ? VW->GetTileWorld() : nullptr;
        if (Mods.PushDist > 0.f)
        {
            int32 DX = (EP.X > Origin.X) ? 1 : ((EP.X < Origin.X) ? -1 : 0);
            int32 DZ = (EP.Z > Origin.Z) ? 1 : ((EP.Z < Origin.Z) ? -1 : 0);
            if (DX == 0 && DZ == 0) DX = 1;  // 對應 Godot：if (dx == 0 && dz == 0) dx = 1
            FGridPos EPos = EP;
            for (int32 S = 0; S < (int32)Mods.PushDist; ++S)
            {
                FGridPos Nxt(EPos.X + DX, EPos.Y, EPos.Z + DZ);
                if (TW_Mods && TW_Mods->GetTile(Nxt.X, Nxt.Y, Nxt.Z) != EMaterialType::Air) break;
                EPos = Nxt;
            }
            if (EPos.X != EP.X || EPos.Z != EP.Z)
                Enemy->SetActorLocation(FVector((float)EPos.X, Enemy->GetActorLocation().Y, (float)EPos.Z),
                                        false, nullptr, ETeleportType::TeleportPhysics);
        }
        if (Mods.PullDist > 0.f)
        {
            const int32 DX = (Origin.X > EP.X) ? 1 : ((Origin.X < EP.X) ? -1 : 0);
            const int32 DZ = (Origin.Z > EP.Z) ? 1 : ((Origin.Z < EP.Z) ? -1 : 0);
            FGridPos EPos = EP;
            for (int32 S = 0; S < (int32)Mods.PullDist; ++S)
            {
                FGridPos Nxt(EPos.X + DX, EPos.Y, EPos.Z + DZ);
                if (TW_Mods && TW_Mods->GetTile(Nxt.X, Nxt.Y, Nxt.Z) != EMaterialType::Air) break;
                EPos = Nxt;
            }
            if (EPos.X != EP.X || EPos.Z != EP.Z)
                Enemy->SetActorLocation(FVector((float)EPos.X, Enemy->GetActorLocation().Y, (float)EPos.Z),
                                        false, nullptr, ETeleportType::TeleportPhysics);
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
        // HpCost 已在函式開頭套用在施法者身上（自損機制），不對敵人重複套用
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
            // 對應 Godot SpellCaster.cs:557-577：fanRange=8+baseR，spread=(d+1)/2 緩慢展開，
            // 僅 Stone 擋格，整排（-spread..spread）都被 Stone 擋住才中斷整個 fan。
            // （PX/PZ 取代 Godot 因 2D→3D 遷移未更新而退化為 0 的 Facing.Y 分量，
            // 以維持「水平扇形」設計意圖，細節見 docs/audit-round3-2026-06-19.md B-7）
            const int32 FanRangeTiles = 8 + BaseR;
            for (int32 d = 1; d <= FanRangeTiles; ++d)
            {
                const int32 Spread = (d + 1) / 2;
                bool bRowBlocked = true;
                for (int32 s = -Spread; s <= Spread; ++s)
                {
                    int32 tx = Origin.X + FX*d + PX*s;
                    int32 tz = Origin.Z + FZ*d + PZ*s;
                    if (!TW->InBounds(tx, Origin.Y, tz)) continue;
                    if (TW->GetTile(tx, Origin.Y, tz) == EMaterialType::Stone_Cobble) continue;
                    bRowBlocked = false;
                    TW->Explode(tx, Origin.Y, tz, 1);
                    if (M.bFire)  SpawnEffectTiles(VW, "fire",  FGridPos(tx, Origin.Y, tz), 1);
                    if (M.bWater) SpawnEffectTiles(VW, "water", FGridPos(tx, Origin.Y, tz), 1);
                }
                if (bRowBlocked) break;
            }
        }
        else if (Shape == "act_area_distant")
        {
            // B-8 修正：Godot SpellCaster.cs:581-583 distRange=16+baseR*2，爆炸半徑 baseR+2
            // 原版 (6+BaseR)*Grain 因 BaseR 已含 Grain 而產生二次縮放，現改為直接對齊 Godot
            int32 dist = 16 + BaseR * 2;
            TW->Explode(Origin.X + FX*dist, Origin.Y, Origin.Z + FZ*dist, BaseR + 2);
        }
        else if (Shape == "act_area_beam")
        {
            // 對應 Godot SpellCaster.cs:588-608：beamLen=18+baseR，三格寬截面（s=-1..1），
            // 僅中心格是 Stone 才中斷整個光束，寬度格各自獨立判斷是否設置。
            const int32 BeamLen = 18 + BaseR;
            for (int32 d = 1; d <= BeamLen; ++d)
            {
                int32 tx = Origin.X + FX*d, tz = Origin.Z + FZ*d;
                if (!TW->InBounds(tx, Origin.Y, tz)) break;
                for (int32 s = -1; s <= 1; ++s)
                {
                    int32 bx = tx + PX*s, bz = tz + PZ*s;
                    if (TW->GetTile(bx, Origin.Y, bz) != EMaterialType::Stone_Cobble)
                        TW->SetTile(bx, Origin.Y, bz, (M.bWater || M.bIce) ? EMaterialType::Water : EMaterialType::Fire);
                }
                if (TW->GetTile(tx, Origin.Y, tz) == EMaterialType::Stone_Cobble) break;
            }
        }
    }

    ApplyModsToNearbyEnemies(M, Origin, VW);
}

void USpellCaster::ExecuteTechnique(const FSpellSlot& Slot, FGridPos Origin,
                                     bool bAtHitPoint, AVoxelWorldActor* VW)
{
    // 對應 Godot SpellCaster.cs:723-809。Godot 的 fx/fy 在 2D→3D 遷移後 fy 恆為 0
    // （PlayerController.Facing 只設 X/Z，見 docs/audit-round3-2026-06-19.md B-10），
    // 故此處以 FX/FZ（水平面）+ PX/PZ（水平垂直分量）取代，維持武技攻擊的水平設計意圖；
    // 半徑/距離公式刻意不乘 Grain，逐行對齊 Godot（Godot 本身也未對此函式做 Grain 縮放）。
    FTileWorld3D* TW = VW ? VW->GetTileWorld() : nullptr;
    if (!TW) return;
    const FSpellMods M = ReadMods(Slot);

    FVector Fwd(1.f, 0.f, 0.f);
    if (ASkillCreatorCharacter* Char = GetOwnerCharacter()) Fwd = Char->GetActorForwardVector();
    const int32 FX = FMath::Abs(Fwd.X) >= FMath::Abs(Fwd.Z) ? (Fwd.X > 0 ? 1 : -1) : 0;
    const int32 FZ = FMath::Abs(Fwd.Z) >  FMath::Abs(Fwd.X) ? (Fwd.Z > 0 ? 1 : -1) : 0;
    const int32 PX = -FZ, PZ = FX;

    const int32 R        = 2 + (int32)(M.DmgBonus * 3.f);   // Godot: r = 2 + (int)(DmgBonus*3)
    const int32 SlashOfs = bAtHitPoint ? 0 : 4;              // Godot: slashOfs
    const FName TotemId  = Slot.TotemId;

    for (int32 Rep = 0; Rep < M.Multi; ++Rep)  // Godot: for (rep=0; rep<m.Multi; rep++)（blue_multi 刻印）
    {
        // ── 新武技技能因子（Design B）────────────────────────────
        if (TotemId == "technique_sword")
        {
            const int32 Ofs = SlashOfs + Rep * 3;
            FGridPos Hit(Origin.X + FX*Ofs, Origin.Y, Origin.Z + FZ*Ofs);
            TW->Explode(Hit.X, Hit.Y, Hit.Z, R);
            if (M.bFire)  SpawnEffectTiles(VW, "fire",  Hit, R);
            if (M.bWater) SpawnEffectTiles(VW, "water", Hit, R);
        }
        else if (TotemId == "technique_punch")
        {
            const int32 PunchR = FMath::Max(1, R - 1);
            const int32 Ofs    = (bAtHitPoint ? 0 : 2) + Rep * 2;
            FGridPos Hit(Origin.X + FX*Ofs, Origin.Y, Origin.Z + FZ*Ofs);
            TW->Explode(Hit.X, Hit.Y, Hit.Z, PunchR);
            if (M.bFire)  SpawnEffectTiles(VW, "fire",  Hit, PunchR);
            if (M.bWater) SpawnEffectTiles(VW, "water", Hit, PunchR);
        }
        else if (TotemId == "technique_shield")
        {
            // TODO-STUB：防禦/反擊，暫以前方衝擊波佔位（對應 Godot SpellCaster.cs:755-759 同註解）
            const int32 Ofs = SlashOfs + Rep * 2;
            FGridPos Hit(Origin.X + FX*Ofs, Origin.Y, Origin.Z + FZ*Ofs);
            TW->Explode(Hit.X, Hit.Y, Hit.Z, R + 1);
        }
        // ── 舊武技技能因子（向後相容）────────────────────────────
        else if (TotemId == "technique_slash")
        {
            const int32 Ofs = SlashOfs + Rep * 3;
            FGridPos Hit(Origin.X + FX*Ofs, Origin.Y, Origin.Z + FZ*Ofs);
            TW->Explode(Hit.X, Hit.Y, Hit.Z, R);
            if (M.bFire)  SpawnEffectTiles(VW, "fire",  Hit, R);
            if (M.bWater) SpawnEffectTiles(VW, "water", Hit, R);
        }
        else if (TotemId == "technique_projectile")
        {
            const int32 Range = 15 + Rep * 5;
            for (int32 i = 1; i <= Range; ++i)
            {
                int32 tx = Origin.X + FX*i, tz = Origin.Z + FZ*i;
                EMaterialType Mat = TW->GetTile(tx, Origin.Y, tz);
                if (Mat == EMaterialType::Stone_Cobble) break;
                TW->SetTile(tx, Origin.Y, tz, (M.bWater || M.bIce) ? EMaterialType::Water : EMaterialType::Fire);
                if (Mat != EMaterialType::Air) break;
            }
        }
        else if (TotemId == "technique_area")
        {
            const int32 Ar = R + 2 + Rep;
            FGridPos Center(Origin.X + FX*Rep*2, Origin.Y, Origin.Z + FZ*Rep*2);
            TW->Explode(Center.X, Center.Y, Center.Z, Ar);
            if (M.bFire)  SpawnEffectTiles(VW, "fire",  Center, Ar);
            if (M.bWater) SpawnEffectTiles(VW, "water", Center, Ar);
            if (!M.bWater && !M.bIce)
                SpawnEffectTiles(VW, "fire", Origin, R);
        }
        else if (TotemId == "technique_beam")
        {
            const int32 Len = 25 + Rep * 8;
            for (int32 i = 1; i <= Len; ++i)
            {
                int32 tx = Origin.X + FX*i, tz = Origin.Z + FZ*i;
                if (!TW->InBounds(tx, Origin.Y, tz)) break;
                for (int32 s = -1; s <= 1; ++s)
                {
                    int32 bx = tx + PX*s, bz = tz + PZ*s;
                    if (TW->GetTile(bx, Origin.Y, bz) != EMaterialType::Stone_Cobble)
                        TW->SetTile(bx, Origin.Y, bz, M.bWater ? EMaterialType::Water : EMaterialType::Fire);
                }
            }
        }
        else if (TotemId == "technique_chain")
        {
            for (int32 c = 0; c < 3 + Rep; ++c)
            {
                const int32 Ofs = 3 + c * 3;
                FGridPos Cp(Origin.X + FX*Ofs, Origin.Y, Origin.Z + FZ*Ofs);
                TW->Explode(Cp.X, Cp.Y, Cp.Z, FMath::Max(1, R - c));
            }
        }
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
    TWeakObjectPtr<USpellCaster>     WeakSelf(this);
    Proj->OnHitEnemy = [Dmg, M, WeakVW2, WeakSelf](AEnemy* Enemy, FGridPos HitPos)
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
        // B-11 修正：命中時觸發 AoE 爆炸 + 刻印位移效果
        // 對應 Godot SpellProjectile.cs:114-116（runner.Submit 在命中點執行整個 spell）
        if (VW2) if (FTileWorld3D* TW2 = VW2->GetTileWorld())
            TW2->Explode(HitPos.X, HitPos.Y, HitPos.Z, 1 + FMath::Max(0, (int32)(M.DmgBonus * 2.f)));
        if (WeakSelf.IsValid())
            WeakSelf->ApplyModsToNearbyEnemies(M, HitPos, VW2);
    };
}

void USpellCaster::ExecuteMorph(const FSpellSlot& Slot, FGridPos Origin, AVoxelWorldActor* VW)
{
    // 對應 Godot SpellCaster.cs:819-836：依 Totem.Id 分四支，各自完全不同的視覺效果
    // （2026-06-20 Round3 B-12 修正：原本不分支，統一塞 Stone 球體，跟變幻 buff 語意無關）
    const FName TotemId = Slot.TotemId;
    if (TotemId == "morph_speed" || TotemId == "morph_strengthen")
    {
        SpawnEffectTiles(VW, "water", Origin, 1);
    }
    else if (TotemId == "morph_flight")
    {
        if (FTileWorld3D* TW = VW ? VW->GetTileWorld() : nullptr)
            TW->Explode(Origin.X, Origin.Y + 3, Origin.Z, 2);
    }
    else if (TotemId == "morph_invisible")
    {
        SpawnEffectTiles(VW, "water", Origin, 2);
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
    else // act_teleport / act_portal（對應 Godot SpellCaster.cs:853-857）
    {
        // B-13 修正：Godot 從最遠（20步）倒數找第一個空氣格，傳送到最遠可達空格
        // 原版向前掃描取最後空氣格，邏輯相近但步數錯誤（=240）且方向語意不同
        constexpr int32 MaxSteps = 20;
        for (int32 i = MaxSteps; i >= 1; --i)
        {
            FGridPos Next(Pos.X + FX*i, Pos.Y, Pos.Z + FZ*i);
            if (!TW || TW->GetTile(Next.X, Next.Y, Next.Z) == EMaterialType::Air)
            {
                Char->TeleportTo(
                    FVector((float)Next.X, Char->GetActorLocation().Y, (float)Next.Z),
                    Char->GetActorRotation());
                break;
            }
        }
    }
}

void USpellCaster::ExecuteSummon(const FSpellSlot& Slot, FGridPos Origin, AVoxelWorldActor* VW)
{
    // 召喚物 AI 本體延遲至 W-11，但補回 Godot SpellCaster.cs:873-892 的視覺占位效果，
    // 避免施放完全無聲無息（2026-06-20 Round3 B-5/B-14 修正：原為純空函式）。
    // Godot 的 fy 因 2D→3D 遷移後恆為 0（同 B-10 註解），故以 FZ 取代 fy 維持水平位移意圖。
    FVector Fwd(1.f, 0.f, 0.f);
    if (ASkillCreatorCharacter* Char = GetOwnerCharacter()) Fwd = Char->GetActorForwardVector();
    const int32 FX = FMath::Abs(Fwd.X) >= FMath::Abs(Fwd.Z) ? (Fwd.X > 0 ? 1 : -1) : 0;
    const int32 FZ = FMath::Abs(Fwd.Z) >  FMath::Abs(Fwd.X) ? (Fwd.Z > 0 ? 1 : -1) : 0;
    FGridPos Sp(Origin.X + FX*4, Origin.Y, Origin.Z + FZ*4);

    const FName TotemId = Slot.TotemId;
    if (TotemId == "summon_minion")
    {
        SpawnEffectTiles(VW, "fire", Sp, 1);
    }
    else if (TotemId == "summon_turret")
    {
        if (FTileWorld3D* TW = VW ? VW->GetTileWorld() : nullptr)
            for (int32 dy = -1; dy <= 1; ++dy)
                TW->SetTile(Sp.X, Sp.Y + dy, Sp.Z, EMaterialType::Stone_Cobble);
    }
    else if (TotemId == "summon_guardian")
    {
        SpawnEffectTiles(VW, "water", Sp, 2);
    }
    UE_LOG(LogTemp, Verbose, TEXT("[SpellCaster] ExecuteSummon AI deferred (W-11): %s"), *Slot.TotemId.ToString());
}

void USpellCaster::ExecuteDomain(const FSpellSlot& Slot, FGridPos Origin, AVoxelWorldActor* VW)
{
    // 對應 Godot SpellCaster.cs:896-918：依 Totem.Id 分三支，三種完全不同效果
    // （2026-06-20 Round3 B-15 修正：原本不分支，統一塞單次大爆炸）。
    // Godot 三個半徑/位移數值均為常數，未乘 Grain/DmgBonus（逐行對齊原始碼）。
    FTileWorld3D* TW = VW ? VW->GetTileWorld() : nullptr;
    if (!TW) return;
    const FSpellMods M = ReadMods(Slot);
    const FName TotemId = Slot.TotemId;

    if (TotemId == "domain_barrier")
    {
        // 環狀護欄：360 度繞圈，每 10 度在水平半徑 8 處設置 Stone（Y 與 Origin 同高）
        for (int32 Angle = 0; Angle < 360; Angle += 10)
        {
            const float Rad = (float)Angle * PI / 180.f;
            const int32 tx = Origin.X + (int32)(FMath::Cos(Rad) * 8.f);
            const int32 tz = Origin.Z + (int32)(FMath::Sin(Rad) * 8.f);
            if (TW->InBounds(tx, Origin.Y, tz))
                TW->SetTile(tx, Origin.Y, tz, EMaterialType::Stone_Cobble);
        }
    }
    else if (TotemId == "domain_terrain")
    {
        TW->Explode(Origin.X, Origin.Y, Origin.Z, 12);
    }
    else if (TotemId == "domain_weather")
    {
        // 雨幕線：沿 X 軸 -8..8，在 Origin.Y-10（較低處）鋪設 Water
        for (int32 dx = -8; dx <= 8; ++dx)
        {
            const int32 tx = Origin.X + dx;
            if (TW->InBounds(tx, Origin.Y - 10, Origin.Z))
                TW->SetTile(tx, Origin.Y - 10, Origin.Z, EMaterialType::Water);
        }
    }

    ApplyModsToNearbyEnemies(M, Origin, VW);
}

// ─────────────────────────────────────────────────────────────────────

void USpellCaster::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (CooldownRemaining > 0.f)
        CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaTime);

    EventBus_ClearFrameIfNeeded();
    Runner.Tick(DeltaTime);
}

// ─────────────────────────────────────────────────────────────────────
//  BuildContext — 從 Spell + 已編譯 Code 建立 FExecutionContext
//  TryCast 和 OnBuildComboContext 共用，避免 delegate 設定重複
// ─────────────────────────────────────────────────────────────────────
TUniquePtr<FExecutionContext> USpellCaster::BuildContext(const FSpellArray& Spell,
                                                         const TArray<FInstruction>& Code)
{
    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    if (!Char) return nullptr;

    FTileWorld3D* TW = CachedVoxelWorld ? CachedVoxelWorld->GetTileWorld() : nullptr;

    auto Ctx     = MakeUnique<FExecutionContext>(Code);
    auto Anchor  = MakeShared<FAnchorState>();

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
        // S-8: 依距離升序排列，確保 QueryNearest 取 [0] 得到最近敵人（Godot SpellCaster.cs:401-406）
        Out.Sort([CP](const FEntityInfo& A, const FEntityInfo& B)
        {
            float dax = (float)(A.Position.X-CP.X), daz = (float)(A.Position.Z-CP.Z);
            float dbx = (float)(B.Position.X-CP.X), dbz = (float)(B.Position.Z-CP.Z);
            return (dax*dax + daz*daz) < (dbx*dbx + dbz*dbz);
        });
        return Out;
    };

    Ctx->RaycastQuery = [TW](FGridPos Start, FVector Dir, float Dist) -> FRaycastResult
    {
        FRaycastResult Out;
        if (!TW) return Out;
        FVector StartF((float)Start.X, (float)Start.Y, (float)Start.Z);
        FRaycastResult3D R = TW->Raycast(StartF, Dir, Dist);
        Out.bHit   = R.bHit;
        Out.HitPos = FGridPos(R.HitCell.X, R.HitCell.Y, R.HitCell.Z);
        Out.MatId  = R.bHit ? (int32)TW->GetTile(R.HitCell.X, R.HitCell.Y, R.HitCell.Z) : 0;
        return Out;
    };

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

    Ctx->PlayerStatsQuery = [Char](FName Key) -> float
    {
        if (Key == "hp")    return Char->CurrentHp;
        if (Key == "mp")    return Char->CurrentMp;
        if (Key == "hpPct") return Char->Stats.MaxHpBase > 0.f ? Char->CurrentHp / Char->Stats.MaxHpBase : 0.f;
        if (Key == "mpPct") return Char->Stats.MaxMpBase > 0.f ? Char->CurrentMp / Char->Stats.MaxMpBase : 0.f;
        return 0.f;
    };

    Ctx->HasSignalFn = [](FName S) { return GEventBusSignals.Contains(S); };
    Ctx->BroadcastFn = [](FName S) { GEventBusSignals.Add(S); };

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

    Ctx->TookDamageThisTickFn = [this]() -> bool
    {
        if (auto* GI = GetWorld()->GetGameInstance())
        if (auto* Sub = GI->GetSubsystem<UCombatStateSubsystem>())
            return Sub->bTookDamageThisFrame;
        return false;
    };

    Ctx->AnchorAction = [TW, Char, Anchor](int32 Radius)
    {
        if (!TW) return;
        FGridPos CP = Char->GetPosition();
        Anchor->Min   = FIntVector(CP.X - Radius, CP.Y - Radius, CP.Z - Radius);
        Anchor->Max   = FIntVector(CP.X + Radius, CP.Y + Radius, CP.Z + Radius);
        Anchor->Cells = TW->SnapshotRegion(Anchor->Min, Anchor->Max);
        Anchor->bValid = true;
    };

    Ctx->RollbackAction = [TW, Anchor]()
    {
        if (!TW || !Anchor->bValid) return;
        TW->RestoreRegion(Anchor->Min, Anchor->Max, Anchor->Cells);
        Anchor->bValid = false;
    };

    Ctx->SetActivationModeFn = [](int32) {};
    Ctx->RegisterFilterFn = [Char](FName FilterType, FName Mode, bool bOneShot,
                                   float Threshold, float CapValue)
    {
        if (Char) Char->ActionBus.RegisterFilter(FilterType, Mode, bOneShot, Threshold, CapValue);
    };

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
        TWeakObjectPtr<USpellCaster>     WeakThis(this);
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

    return Ctx;
}
