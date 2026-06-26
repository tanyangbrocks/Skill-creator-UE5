#include "ASkillCreatorCharacter.h"
#include "APhysicalItemActor.h"
#include "FCombatResolver.h"
#include "UCombatantRegistrySubsystem.h"
#include "UCharacterStateComponent.h"
#include "UElementalAuraComponent.h"
#include "USpellCaster.h"
#include "UInventoryComponent.h"
#include "UEquipmentComponent.h"
#include "UCombatStateSubsystem.h"
#include "UGameClockSubsystem.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "AEnemyManager.h"
#include "ExecutionContext.h"
#include "USnapshotManager.h"
#include "AEnemy.h"
#include "ABeastCharacter.h"
#include "ANPCCharacter.h"
#include "GridPos.h"
#include "UDroppedItemManager.h"
#include "UEquipmentComponent.h"
#include "ItemRegistry.h"
#include "ItemData.h"
#include "EquipmentSlot.h"
#include "WorldScale.h"
#include "CharacterSaveData.h"
#include "AFloatingDamageActor.h"
#include "SpellSaveSystem.h"
#include "ASkillCreatorHUD.h"
#include "PlacementShape.h"
#include "PlacementValidator.h"
#include "PlacedObjectRegistry.h"
#include "PlacedUnit.h"
#include "MaterialRegistry.h"
#include "IInteractable.h"
#include "ICollectible.h"
#include "AWeedEntity.h"
#include "Engine/OverlapResult.h"
#include "UPotionBagComponent.h"
#include "UMapComponent.h"
#include "UAfterimageFXComponent.h"
#include "DrawDebugHelpers.h"
#include "InputTriggers.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Math/RotationMatrix.h"
#include "InputCoreTypes.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "InputModifiers.h"

ASkillCreatorCharacter::ASkillCreatorCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // 碰撞膠囊：依 WorldScale 尺度，改 TileSizeCm 自動跟進
    GetCapsuleComponent()->InitCapsuleSize(
        WorldScale::CapsuleRadius,
        WorldScale::CapsuleHalfHeight
    );

    // 移動速度：依 WorldScale 尺度
    GetCharacterMovement()->MaxWalkSpeed  = WorldScale::WalkSpeedCm;
    GetCharacterMovement()->JumpZVelocity = WorldScale::JumpZVelocityCm;
    GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
    // GravityScale 縮放：使「3 tile 跳躍高度」在任意 GrainCurrent 下保持一致（tile 單位）。
    // 不設定 → Grain=16 時有效重力過強，跳躍高度僅 4 cm（物理公式：h = v²/2g）。
    GetCharacterMovement()->GravityScale  = WorldScale::GravityScaleMult;

    // 鏡頭 rig：臂長與偏移依 WorldScale 尺度
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength         = WorldScale::CameraArmLength;
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->SetRelativeLocation(FVector(0.f, 0.f, WorldScale::CameraArmZOffset));

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;
    Camera->FieldOfView = 70.f;

    bUseControllerRotationYaw   = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll  = false;

    StateComp       = CreateDefaultSubobject<UCharacterStateComponent>(TEXT("StateComp"));
    AuraComp        = CreateDefaultSubobject<UElementalAuraComponent>(TEXT("AuraComp"));
    SpellCasterComp = CreateDefaultSubobject<USpellCaster>(TEXT("SpellCasterComp"));
    InventoryComp   = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComp"));
    EquipmentComp   = CreateDefaultSubobject<UEquipmentComponent>(TEXT("EquipmentComp"));
    PotionBagComp   = CreateDefaultSubobject<UPotionBagComponent>(TEXT("PotionBagComp"));
    MapComp         = CreateDefaultSubobject<UMapComponent>(TEXT("MapComp"));
    AfterimageComp  = CreateDefaultSubobject<UAfterimageFXComponent>(TEXT("AfterimageComp"));
}

void ASkillCreatorCharacter::BeginPlay()
{
    Super::BeginPlay();
    CurrentHp = Stats.MaxHpBase;
    CurrentMp = Stats.MaxMpBase;

    // W-6: 初始化預設法力插槽（"gui_dao" = 主法力池，對應 Godot Mp）
    ActiveManaSlots.Reset();
    ActiveManaSlots.Add(FManaSlot(TEXT("gui_dao"), Stats.MaxMpBase, Stats.MpRegenRate));

    RebindWorldSystems();

    // T-01: 清除跨 PIE session 殘留的全域狀態
    FExecutionContext::GlobalVars.Empty();
    FExecutionContext::GlobalLists.Empty();
    FExecutionContext::TaskCounters.Empty();
    FExecutionContext::TaskCounterReached.Empty();
    FExecutionContext::bTraceMode = false;

    // B-4: HP/MP 每 0.5s 一跳的 regen 計時器（對應設計文件「每 0.5 秒恢復的量」）
    // 速率單位為「/秒」（Godot CharacterStats.cs），所以每跳應用 rate * 0.5f
    GetWorldTimerManager().SetTimer(RegenTimerHandle, this,
        &ASkillCreatorCharacter::TickRegen, 0.5f, /*bLoop=*/true);

    if (UCombatantRegistrySubsystem* Reg = GetWorld()->GetSubsystem<UCombatantRegistrySubsystem>())
        Reg->Register(this);
}

void ASkillCreatorCharacter::EndPlay(EEndPlayReason::Type Reason)
{
    if (UCombatantRegistrySubsystem* Reg = GetWorld() ? GetWorld()->GetSubsystem<UCombatantRegistrySubsystem>() : nullptr)
        Reg->Unregister(this);
    Super::EndPlay(Reason);
}

void ASkillCreatorCharacter::RebindWorldSystems()
{
    CachedVoxelWorld = AVoxelWorldActor::FindInWorld(GetWorld());

    // 快取 EnemyManager（關卡可能手動放置或由 GameMode 自動生成）
    CachedEnemyMgr = nullptr;
    for (TActorIterator<AEnemyManager> It(GetWorld()); It; ++It)
    { CachedEnemyMgr = *It; break; }

    // 綁定採掘掉落回呼（從 SkillCreatorRuntime 側接入，避免循環依賴）
    if (CachedVoxelWorld)
    if (FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld())
    {
        TWeakObjectPtr<UWorld>            WeakWorld(GetWorld());
        TWeakObjectPtr<AVoxelWorldActor>  WeakVoxelWorld(CachedVoxelWorld);
        // W-C：樹木倒塌狀態旗標（lambda 共享，防止 BFS→DestroyTile 遞迴觸發二次 BFS）
        TSharedPtr<bool> bInTreeCollapse = MakeShared<bool>(false);

        TW->OnTileDestroyed = [WeakWorld, WeakVoxelWorld, bInTreeCollapse]
            (int32 x, int32 y, int32 z, EMaterialType OldMat, EDestroyReason Reason)
        {
            UWorld* W = WeakWorld.Get();
            if (!W) return;

            // K-5：任何方式摧毀的格子都要通知 Registry（不限完美移除路徑），
            // 對應 Godot Main.cs:404-407 全域 OnTileDestroyed → NotifyDestroyed
            if (AVoxelWorldActor* VW = WeakVoxelWorld.Get())
            {
                VW->GetPlacedRegistry().NotifyDestroyed(FIntVector(x, y, z));
                // W-E：Grass tile 被挖掉，露出正下方的 Dirt_Dry → 開始 180s 回復計時
                if (OldMat == EMaterialType::Grass)
                    VW->NotifyDirtExposed(FIntVector(x, y + 1, z));  // UE5 Y 增大=向下，+1 是正下方
            }

            auto* DropMgr = W->GetSubsystem<UDroppedItemManager>();
            if (!DropMgr) return;

            // W-C：採掘樹幹 → BFS 連帶崩塌整棵樹（純 tile 設計，無 TreeActor）
            if (OldMat == EMaterialType::Wood && Reason == EDestroyReason::Mining && !(*bInTreeCollapse))
            {
                if (AVoxelWorldActor* VW = WeakVoxelWorld.Get())
                {
                    if (FTileWorld3D* TileWorld = VW->GetTileWorld())
                    {
                        *bInTreeCollapse = true;

                        // BFS：Root 已是 Air，從 6 方向鄰居找連通 Wood/Leaves
                        TQueue<FIntVector>  BFSQueue;
                        TSet<FIntVector>    Visited;
                        const FIntVector    Root(x, y, z);
                        Visited.Add(Root);

                        static const FIntVector Dirs6[] = {
                            {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}
                        };
                        for (const FIntVector& D : Dirs6)
                        {
                            FIntVector N = Root + D;
                            EMaterialType NM = TileWorld->GetTile(N.X, N.Y, N.Z);
                            if ((NM == EMaterialType::Wood || NM == EMaterialType::Leaves) && !Visited.Contains(N))
                            { Visited.Add(N); BFSQueue.Enqueue(N); }
                        }

                        TArray<FIntVector> ToRemove;
                        while (!BFSQueue.IsEmpty())
                        {
                            FIntVector Cur; BFSQueue.Dequeue(Cur);
                            ToRemove.Add(Cur);
                            for (const FIntVector& D : Dirs6)
                            {
                                FIntVector N = Cur + D;
                                if (Visited.Contains(N)) continue;
                                EMaterialType NM = TileWorld->GetTile(N.X, N.Y, N.Z);
                                if (NM == EMaterialType::Wood || NM == EMaterialType::Leaves)
                                { Visited.Add(N); BFSQueue.Enqueue(N); }
                            }
                        }

                        // DestroyTile 觸發遞迴 OnTileDestroyed → bInTreeCollapse=true → 走一般掉落路徑
                        for (const FIntVector& T : ToRemove)
                            TileWorld->DestroyTile(T.X, T.Y, T.Z, EDestroyReason::Mining);

                        *bInTreeCollapse = false;

                        // 原始採掘格補掉落（本次 callback 不走下方一般路徑）
                        DropMgr->SpawnForReason(x, y, z, OldMat, Reason);
                        return;
                    }
                }
            }

            // 一般掉落路徑（非樹木倒塌，或樹木倒塌期間的遞迴呼叫）
            DropMgr->SpawnForReason(x, y, z, OldMat, Reason);
        };

        // D-3：爆炸聚合回呼 → 按材質批次 spawn ADebrisActor
        TW->OnExplodeComplete = [WeakWorld](FIntVector Center,
                                             const TMap<EMaterialType, int32>& DestroyedByMat)
        {
            UWorld* W = WeakWorld.Get();
            if (!W) return;
            auto* DropMgr = W->GetSubsystem<UDroppedItemManager>();
            if (!DropMgr) return;

            FDebrisParams P;
            P.Reason    = EDestroyReason::Explosion;
            P.Intensity = 1.f;  // 未來由技能強度傳入
            for (const auto& Pair : DestroyedByMat)
                DropMgr->SpawnDebris(Center, Pair.Key, Pair.Value, P);
        };
    }
}

void ASkillCreatorCharacter::FillSaveData(FCharacterSaveData& OutData) const
{
    OutData.Level    = Level;
    OutData.Xp       = Xp;
    OutData.CurrentHp = IsAlive() ? CurrentHp : Stats.MaxHpBase;  // D-1: 死亡時存 MaxHp，不存 0
    OutData.CurrentMp = CurrentMp;

    FGridPos Pos = GetPosition();
    OutData.TilePosition = FIntVector(Pos.X, Pos.Y, Pos.Z);

    if (InventoryComp)
        OutData.InventorySlots = InventoryComp->Slots;

    if (StateComp)
    {
        OutData.Stamina      = StateComp->Stamina;
        OutData.MentalEnergy = StateComp->MentalEnergy;
        OutData.Mood         = StateComp->Mood;
    }

    OutData.ManaCurrents.Empty();
    for (const FManaSlot& Slot : ActiveManaSlots)
        OutData.ManaCurrents.Add(Slot.ManaTypeKey, Slot.Current);

    if (SpellCasterComp)
        OutData.SpellGroupJson = FSpellSaveSystem::SaveGroupToString(SpellCasterComp->SpellGroups);
}

void ASkillCreatorCharacter::ApplyCharacterSaveData(const FCharacterSaveData& Data)
{
    Level     = Data.Level;
    Xp        = Data.Xp;
    CurrentHp = FMath::Max(1.f, Data.CurrentHp);  // D-1 guard: 防止以 0 HP 載入（Godot Main.cs:355）
    CurrentMp = Data.CurrentMp;

    if (InventoryComp)
        InventoryComp->Slots = Data.InventorySlots;

    if (StateComp)
    {
        StateComp->Stamina      = Data.Stamina;
        StateComp->MentalEnergy = Data.MentalEnergy;
        StateComp->Mood         = Data.Mood;
    }

    for (const TPair<FName, float>& Pair : Data.ManaCurrents)
        if (FManaSlot* Slot = GetManaSlot(Pair.Key))
            Slot->Current = Pair.Value;

    if (SpellCasterComp && !Data.SpellGroupJson.IsEmpty())
        FSpellSaveSystem::LoadGroupFromString(Data.SpellGroupJson, SpellCasterComp->SpellGroups);

    // B-1 + 設計問題1：W-10 基礎能力點存檔→Stats，並計算衍生值
    // 暫定公式：1點肌力 = 20力量（無種族係數，用戶確認）
    Stats.ConstitutionPoints = Data.BasePoint_Physique;
    Stats.StrengthPoints     = Data.BasePoint_Strength;
    Stats.EndurancePoints    = Data.BasePoint_Endurance;
    Stats.AgilityPoints      = Data.BasePoint_Agility;
    Stats.WisdomPoints       = Data.BasePoint_Intellect;
    Stats.CharismaPoints     = Data.BasePoint_Charisma;
    Stats.LuckPoints         = Data.BasePoint_Luck;
    Stats.Strength           = Stats.StrengthPoints * 20.f;

    OnHpChanged.Broadcast(CurrentHp);
}

void ASkillCreatorCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // W-6: 法力插槽 Tick（slot[0] 同步 Stats 上限與回復率，所有插槽逐幀回復）
    if (ActiveManaSlots.Num() > 0)
    {
        ActiveManaSlots[0].Max      = Stats.MaxMpBase;
        ActiveManaSlots[0].RegenRate = Stats.MpRegenRate;
    }
    // MP/HP regen 移至 B-4 TickRegen()（0.5s 計時器）；此處只同步 CurrentMp 供 HUD 讀取
    if (ActiveManaSlots.Num() > 0) CurrentMp = ActiveManaSlots[0].Current;

    // CharacterState Tick（體力、精力、心情）
    bool bInCombat = false;
    if (auto* GI = GetWorld()->GetGameInstance())
    {
        if (auto* Sub = GI->GetSubsystem<UCombatStateSubsystem>())
        {
            bInCombat = Sub->bInCombat;
            Sub->Advance(DeltaTime);
        }
        if (auto* Clock = GI->GetSubsystem<UGameClockSubsystem>())
            Clock->Advance(DeltaTime);
    }

    float SurvivalDamage = StateComp->TickState(DeltaTime, bInCombat);
    if (SurvivalDamage > 0.f)
        TakeDirectDamage(SurvivalDamage);

    AuraComp->Process(DeltaTime);
    ActionBus.Update(DeltaTime);

    ApplyEnvironmentalDamage(DeltaTime);

    // S-3：鎖敵相機追蹤（自動解鎖：死亡/超距離）
    if (LockedTarget)
    {
        const float MaxLockDistCm = WorldScale::TileSizeCm * WorldScale::GrainCurrent * 30.f;
        if (!LockedTarget->IsAlive() ||
            FVector::Dist(GetActorLocation(), LockedTarget->GetActorLocation()) > MaxLockDistCm)
        {
            LockedTarget = nullptr;
        }
        else if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            const FVector ToTarget = LockedTarget->GetActorLocation() - GetActorLocation();
            const FRotator Want(PC->GetControlRotation().Pitch, ToTarget.Rotation().Yaw, 0.f);
            PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), Want, DeltaTime, 5.f));
        }
    }

    // 自動拾取玩家周圍掉落物，並在裝備槽空閒時自動穿戴
    if (auto* DropMgr = GetWorld()->GetSubsystem<UDroppedItemManager>())
    {
        TArray<FItemStack> Picked = DropMgr->TryPickupAll(this);
        for (const FItemStack& S : Picked)
        {
            if (!InventoryComp || S.ItemId == EItemId::None) continue;
            int32 SlotIdx = InventoryComp->TryAdd(S.ItemId, S.Count);
            if (SlotIdx >= 0 && EquipmentComp)
            {
                const FItemData& D = FItemRegistry::Get(S.ItemId);
                // 2026-06-23：裝備欄改成 FName 動態欄位，自由欄位判斷直接查 EquippedBySlot
                const bool bFree = !D.EquipSlot.IsNone() && EquipmentComp->GetEquipped(D.EquipSlot) == EItemId::None;
                if (bFree)
                    EquipmentComp->TryEquip(InventoryComp, SlotIdx);
            }
        }
    }

    // 互動系統：每幀追蹤玩家視線，找最近的 IInteractable Actor（docs/plan-item-crafting-system.md §五）
    UpdateInteractableTrace();

    // 採掘高亮：每幀追蹤玩家視線，顯示即將被採掘的整個範圍（不只是中心 1 格）
    if (CachedVoxelWorld)
        CachedVoxelWorld->ShowHighlight(ComputeHighlightTiles());

    // 放置冷卻：每幀減算（對應 Godot Main.cs _process 中 PlaceCooldown 每幀倒數，不依賴右鍵是否按住）
    if (PlaceCooldown > 0.f)
        PlaceCooldown -= DeltaTime;

    // ── F2：座標偵錯 overlay（對應 Godot Main.cs:1023-1031）──────────
    if (bDebugCoordEnabled && GEngine)
    {
        const FGridPos TilePos  = GetPosition();
        const FVector  WorldPos = GetActorLocation();

        APlayerController* DbgPC = Cast<APlayerController>(GetController());
        float MX = 0.f, MY = 0.f;
        if (DbgPC) DbgPC->GetMousePosition(MX, MY);

        // 鏡頭前方 5 格瞄準格（對應 Godot _player.MouseGridPos）
        FGridPos LookTile = TilePos;
        if (DbgPC && CachedVoxelWorld)
        {
            FVector CamLoc; FRotator CamRot;
            DbgPC->GetPlayerViewPoint(CamLoc, CamRot);
            const FVector LookWorld = CamLoc + CamRot.Vector() * (5.f * WorldScale::TileSizeCm);
            const int32 TH = CachedVoxelWorld->GetTileWorld()
                           ? CachedVoxelWorld->GetTileWorld()->Height : 256;
            LookTile = WorldScale::WorldToTile(LookWorld, TH);
        }

        // 視角模式名稱（對應 Godot _camera3d.Mode）
        const TCHAR* ModeStr = TEXT("?");
        switch (CameraMode) {
            case ECameraMode::ThirdPerson:  ModeStr = TEXT("第三人稱"); break;
            case ECameraMode::FirstPerson:  ModeStr = TEXT("第一人稱"); break;
            case ECameraMode::Isometric:    ModeStr = TEXT("等角視角"); break;
            case ECameraMode::SideScroll2D: ModeStr = TEXT("橫向2D"); break; // 對應 Godot CameraMode.SideScroll2D，原始碼/文件從未出現「2.5D」這個說法
        }

        // 羅盤方向（對應 Godot ddx/ddy）
        const float Yaw = GetControlRotation().Yaw;
        const float Canon = FMath::Fmod(Yaw + 360.f, 360.f);
        const TCHAR* DirStr =
            (Canon < 45.f || Canon >= 315.f) ? TEXT("北") :
            (Canon < 135.f)                  ? TEXT("東") :
            (Canon < 225.f)                  ? TEXT("南") : TEXT("西");

        const float ArmLen = SpringArm ? SpringArm->TargetArmLength : 0.f;

        GEngine->AddOnScreenDebugMessage(9901, 0.f, FColor::Green, FString::Printf(
            TEXT("[偵錯 F2]\n")
            TEXT("視角:   %s\n")
            TEXT("螢幕:   (%.0f, %.0f) px\n")
            TEXT("世界:   (%.1f, %.1f, %.1f)\n")
            TEXT("瞄準格: (%d, %d, %d)\n")
            TEXT("玩家格: (%d, %d, %d)\n")
            TEXT("方向:   %s  (yaw %.1f°)\n")
            TEXT("彈簧臂: %.0f cm"),
            ModeStr, MX, MY,
            WorldPos.X, WorldPos.Y, WorldPos.Z,
            LookTile.X, LookTile.Y, LookTile.Z,
            TilePos.X, TilePos.Y, TilePos.Z,
            DirStr, Yaw, ArmLen));
    }

    // ── F4：生存速率偵錯 overlay ──────────────────────────────────────
    if (bDebugSurvivalEnabled && StateComp && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(9902, 0.f, FColor::Orange,
            FString::Printf(
                TEXT("[偵錯 F4] 體力:%.1f  精力:%.1f  心情:%.1f\n  HP:%.1f/%.1f  MP:%.1f"),
                StateComp->Stamina, StateComp->MentalEnergy, StateComp->Mood,
                CurrentHp, Stats.MaxHpBase, CurrentMp));
    }

    // S-2 蓄力計時
    if (bChargingAttack)
        AttackChargeTimer = FMath::Min(AttackChargeTimer + DeltaTime, MaxChargeTime);

    // 緩降：下落速度超過 3× 步行速度時限制終端速度（待平衡：觸發高度/速度閾值）
    if (GetCharacterMovement()->IsFalling())
    {
        FVector Vel = GetVelocity();
        const float MaxFallSpeed = WorldScale::WalkSpeedCm * 3.f;
        if (Vel.Z < -MaxFallSpeed)
        {
            Vel.Z = -MaxFallSpeed;
            GetCharacterMovement()->Velocity = Vel;
        }
    }

    // W-G：雜草 random tick
    TickWeedGrowth(DeltaTime);
}

float ASkillCreatorCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    // 暫時套用物理防禦管線（B-1g）；B-3 完整管線用 TakePhysicalDamage/TakeEnergyDamage
    float Modified     = DamageAmount * (1.f + AuraComp->DamageTakenBonus);
    float Defense      = Stats.PhysicalDefense * (1.f - AuraComp->DefensePenalty);
    float AfterDef     = FMath::Max(0.f, Modified - Defense);
    float Final        = FMath::Max(0.f, AfterDef - Stats.PhysicalDamageReduction);
    TakeDirectDamage(Final);
    return Final;
}

// B-3：物理傷害管線（S-4 彈反/防禦 → 防禦/減傷 → 暴擊判定 → 命中/閃避）
// 對應 Godot 設計文件 base value system.txt 第 24-29 行物理 2 步公式
// ── ICombatant 方法 ─────────────────────────────────────────────────────────

UElementalAuraComponent* ASkillCreatorCharacter::GetAuraComp() const { return AuraComp; }

void ASkillCreatorCharacter::ApplyFinalDamage(float FinalDmg) { TakeDirectDamage(FinalDmg); }

void ASkillCreatorCharacter::TakePhysicalDamage(float PhysAtk, const FCharacterStats* Atk, AActor* Attacker)
{
    // S-4 防禦/彈反判定（先於命中/閃避計算）
    if (IsGuarding())
    {
        if (bInParryWindow)
        {
            // 彈反成功：完全無傷 + 震暈近戰攻擊者 3 秒
            bInParryWindow = false;
            GetWorldTimerManager().ClearTimer(ParryWindowTimer);
            if (ABeastCharacter* BeastAtk = Cast<ABeastCharacter>(Attacker))
                if (BeastAtk->AuraComp)
                    BeastAtk->AuraComp->ApplyFreeze(3.f, BeastAtk);
            OnParrySuccess.Broadcast(Attacker);
            return;
        }
        // 防禦中但彈反窗口已過：50% 物理傷害
        PhysAtk *= 0.5f;
    }

    if (!FCombatResolver::TakePhysicalDamage(*this, PhysAtk, Atk))
        return;  // miss / dodge：跳過元素接觸（未命中不接觸）

    // 元素接觸：攻擊者 NativeElement → 玩家；玩家防具 Element → 攻擊者
    if (AuraComp)
    {
        // 攻擊者帶有 NativeElement（Beast/NPC 屬性）→ 施加到玩家
        if (ABeastCharacter* Beast = Cast<ABeastCharacter>(Attacker))
        {
            if (Beast->AuraComp && Beast->AuraComp->NativeElement != ESkillElementType::None)
                AuraComp->ApplyImmediate(Beast->AuraComp->NativeElement,
                    UElementalAuraComponent::DefaultAuraDuration, this);
        }
        else if (ANPCCharacter* NPC = Cast<ANPCCharacter>(Attacker))
        {
            if (NPC->AuraComp && NPC->AuraComp->NativeElement != ESkillElementType::None)
                AuraComp->ApplyImmediate(NPC->AuraComp->NativeElement,
                    UElementalAuraComponent::DefaultAuraDuration, this);
        }

        // 裝備防具/飾品元素 → 攻擊者（「碰到我等於碰到這個元素」）
        if (EquipmentComp && Attacker)
        {
            auto ApplyArmorElem = [&](FName Slot)
            {
                EItemId Id = EquipmentComp->GetEquipped(Slot);
                if (Id == EItemId::None) return;
                ESkillElementType Elem = FItemRegistry::Get(Id).Element;
                if (Elem == ESkillElementType::None) return;
                if (ABeastCharacter* B = Cast<ABeastCharacter>(Attacker))
                    B->AuraComp->ApplyImmediate(Elem, UElementalAuraComponent::DefaultAuraDuration, B);
                else if (ANPCCharacter* N = Cast<ANPCCharacter>(Attacker))
                    N->AuraComp->ApplyImmediate(Elem, UElementalAuraComponent::DefaultAuraDuration, N);
            };
            ApplyArmorElem(FName("Armor"));
            ApplyArmorElem(FName("Accessory"));
        }
    }
}

void ASkillCreatorCharacter::TakeEnergyDamage(float EnergyAtk, FName ManaTypeKey, const FCharacterStats* Atk)
{
    FCombatResolver::TakeEnergyDamage(*this, EnergyAtk, ManaTypeKey, Atk);
}

void ASkillCreatorCharacter::TakeElementalDamage(float ElemAtk, ESkillElementType Element, bool bEnergyDefenseApplies, const FCharacterStats* Atk)
{
    FCombatResolver::TakeElementalDamage(*this, ElemAtk, Element, bEnergyDefenseApplies, Atk);
}

// B-4：HP/MP 0.5s regen（速率單位為「/秒」，每跳 × 0.5f）
void ASkillCreatorCharacter::TickRegen()
{
    if (!IsAlive()) return;

    if (Stats.HpRegenRate > 0.f && CurrentHp < Stats.MaxHpBase)
        CurrentHp = FMath::Min(Stats.MaxHpBase, CurrentHp + Stats.HpRegenRate * 0.5f);

    for (FManaSlot& Slot : ActiveManaSlots)
        Slot.Current = FMath::Min(Slot.Max, Slot.Current + Slot.RegenRate * 0.5f);
}

void ASkillCreatorCharacter::TakeDirectDamage(float Amount)
{
    if (CurrentHp <= 0.f) return;

    // DamageShield 攔截：可能將傷害歸零或縮減
    float Final = ActionBus.DispatchPlayerDamage(Amount);

    CurrentHp = FMath::Max(0.f, CurrentHp - Final);
    OnHpChanged.Broadcast(CurrentHp);

    if (Final > 0.f)
        AFloatingDamageActor::Spawn(GetWorld(),
            GetActorLocation() + FVector(0.f, 0.f, 50.f), Final);

    if (auto* GI = GetWorld()->GetGameInstance())
        if (auto* Sub = GI->GetSubsystem<UCombatStateSubsystem>())
        {
            Sub->OnPlayerTookDamage();
            Sub->OnHit.Broadcast(GetPosition(), Final, true);
        }

    if (CurrentHp <= 0.f)
    {
        // DeathGuard 攔截：取消死亡，保留 1 HP
        if (ActionBus.DispatchPlayerDeath())
        {
            CurrentHp = 1.f;
            OnHpChanged.Broadcast(CurrentHp);
        }
        else
        {
            OnCharacterDied.Broadcast();
        }
    }
}

FEntitySnapshot ASkillCreatorCharacter::TakeSnapshot() const
{
    FEntitySnapshot Snap;
    Snap.EntityId      = FEntitySnapshot::PlayerEntityId;
    Snap.Position      = GetPosition();
    Snap.Hp            = CurrentHp;
    Snap.Mp            = CurrentMp;
    Snap.bWasAlive     = IsAlive();
    Snap.bHasCharStats = true;
    Snap.CharStats     = Stats;
    return Snap;
}

void ASkillCreatorCharacter::RestoreFromSnapshot(const FEntitySnapshot& Snap)
{
    CurrentHp = Snap.Hp;
    CurrentMp = Snap.Mp;
    if (Snap.bHasCharStats)
        Stats = Snap.CharStats;
    if (ActiveManaSlots.Num() > 0)
        ActiveManaSlots[0].Current = CurrentMp;
    OnHpChanged.Broadcast(CurrentHp);
    const int32 WorldH = CachedVoxelWorld
        ? CachedVoxelWorld->GetTileWorld()->Height
        : WorldScale::DefaultWorldHeight;
    SetActorLocation(WorldScale::TileToWorld(Snap.Position, WorldH));
}

FGridPos ASkillCreatorCharacter::GetPosition() const
{
    int32 WorldH = WorldScale::DefaultWorldHeight;
    if (CachedVoxelWorld)
        if (FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld())
            WorldH = TW->Height;
    return WorldScale::WorldToTile(GetActorLocation(), WorldH);
}

void ASkillCreatorCharacter::ApplyEnvironmentalDamage(float DeltaTime)
{
    if (CurrentHp <= 0.f || !CachedVoxelWorld) return;
    FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld();
    if (!TW) return;

    const FGridPos Pos = WorldScale::WorldToTile(GetActorLocation(), TW->Height);
    const EMaterialType CurTileMat  = TW->GetTile(Pos.X, Pos.Y, Pos.Z);     // 身體所在格
    const EMaterialType FloorMat    = TW->GetTile(Pos.X, Pos.Y + 1, Pos.Z); // 腳下格（UE5 Y-down）
    const FMaterialData& CurData    = FMaterialRegistry::Get(CurTileMat);
    const FMaterialData& FloorData  = FMaterialRegistry::Get(FloorMat);

    // 原有 DPS（Fire/Lava 接觸傷害）
    float Dps = 0.f;
    if (CurTileMat == EMaterialType::Fire)  Dps = AEnemyManager::FireDps;
    else if (CurTileMat == EMaterialType::Lava) Dps = AEnemyManager::LavaDps;
    if (Dps > 0.f) TakeDirectDamage(Dps * DeltaTime);

    // P-12: ContactEffect — 材質接觸元素效果（帶 1 秒冷卻，避免每幀重疊施加）
    if (AuraComp && CurData.ContactEffect != EContactEffect::None)
    {
        ESkillElementType ContactElem = ESkillElementType::None;
        switch (CurData.ContactEffect)
        {
            case EContactEffect::Burning:     ContactElem = ESkillElementType::Fire;    break;
            case EContactEffect::Wet:         ContactElem = ESkillElementType::Water;   break;
            case EContactEffect::Poison:      ContactElem = ESkillElementType::Poison;  break;
            case EContactEffect::Electric:    ContactElem = ESkillElementType::Thunder; break;
            case EContactEffect::Frozen:      ContactElem = ESkillElementType::Ice;     break;
            case EContactEffect::Radioactive: ContactElem = ESkillElementType::Poison;  break;
            default: break;
        }
        if (ContactElem != ESkillElementType::None)
            AuraComp->Apply(ContactElem, 3.f, this);
    }

    // P-13/P-14/P-15: 僅在非飛行狀態下修改地面移速與摩擦
    if (!IsFlying())
    {
        // P-13: SpeedFactor（腳下材質）× P-14: (1−Stickyness)（身體所在液體阻尼）
        const float SpeedMult = FloorData.SpeedFactor * (1.f - CurData.Stickyness);
        float BaseSpeed = WorldScale::WalkSpeedCm;
        switch (MovementState)
        {
            case EPlayerMovementState::Sprinting:      BaseSpeed = WorldScale::WalkSpeedCm * 2.f;  break;
            case EPlayerMovementState::SuperSprinting: BaseSpeed = WorldScale::WalkSpeedCm * 4.f;  break;
            case EPlayerMovementState::Rolling:        BaseSpeed = WorldScale::WalkSpeedCm * 1.5f; break;
            case EPlayerMovementState::Sliding:        BaseSpeed = WorldScale::WalkSpeedCm * 2.f;  break;
            default: break;
        }
        GetCharacterMovement()->MaxWalkSpeed = FMath::Max(0.f, BaseSpeed * SpeedMult);
        // P-15: Slippery（腳下材質）→ 地面摩擦係數（8.0 = 預設急停；0 = 完全無摩擦）
        GetCharacterMovement()->GroundFriction = 8.f * (1.f - FloorData.Slippery);
    }
}

FManaSlot* ASkillCreatorCharacter::GetManaSlot(FName Key)
{
    for (FManaSlot& Slot : ActiveManaSlots)
        if (Slot.ManaTypeKey == Key) return &Slot;
    return nullptr;
}

void ASkillCreatorCharacter::GainXp(float Amount)
{
    Xp += Amount;
    while (Xp >= (float)XpRequired(Level))
    {
        Xp -= (float)XpRequired(Level);
        ++Level;
    }
}

FString ASkillCreatorCharacter::GetTierName(int32 InLevel)
{
    if (InLevel < 10)  return TEXT("煉氣");
    if (InLevel < 20)  return TEXT("築基");
    if (InLevel < 35)  return TEXT("化形");
    if (InLevel < 50)  return TEXT("金丹");
    if (InLevel < 65)  return TEXT("元嬰");
    if (InLevel < 80)  return TEXT("出竅");
    if (InLevel < 100) return TEXT("散仙");
    return TEXT("真仙");
}

void ASkillCreatorCharacter::SetupPlayerInputComponent(UInputComponent* PlayerIC)
{
    Super::SetupPlayerInputComponent(PlayerIC);
    UEnhancedInputComponent* EIC = CastChecked<UEnhancedInputComponent>(PlayerIC);

    // ── 建立 InputActions（純 C++，不需要 .uasset）────────────────────────
    auto MakeBool = [this](FName Name) -> UInputAction*
    {
        UInputAction* IA = NewObject<UInputAction>(this, Name);
        IA->ValueType = EInputActionValueType::Boolean;
        return IA;
    };

    UInputAction* IA_Jump      = MakeBool(TEXT("Jump"));
    UInputAction* IA_Mine      = MakeBool(TEXT("Mine"));
    UInputAction* IA_PrimaryTap = MakeBool(TEXT("PrimaryTap"));
    UInputAction* IA_Place     = MakeBool(TEXT("Place"));
    UInputAction* IA_SpellU    = MakeBool(TEXT("SpellU"));
    UInputAction* IA_SpellI    = MakeBool(TEXT("SpellI"));
    UInputAction* IA_SpellO    = MakeBool(TEXT("SpellO"));
    UInputAction* IA_SpellP    = MakeBool(TEXT("SpellP"));
    UInputAction* IA_Camera    = MakeBool(TEXT("ToggleCamera"));
    UInputAction* IA_DbgTrace  = MakeBool(TEXT("DebugTrace"));
    UInputAction* IA_SnapTake  = MakeBool(TEXT("SnapshotTake"));
    UInputAction* IA_SnapApply = MakeBool(TEXT("SnapshotApply"));
    UInputAction* IA_DbgPaint    = MakeBool(TEXT("DbgPaint"));
    UInputAction* IA_DbgCoord    = MakeBool(TEXT("DbgCoord"));
    UInputAction* IA_DbgSurvival = MakeBool(TEXT("DbgSurvival"));

    UInputAction* IA_Move = NewObject<UInputAction>(this, TEXT("IA_Move"));
    IA_Move->ValueType = EInputActionValueType::Axis2D;

    UInputAction* IA_Look = NewObject<UInputAction>(this, TEXT("IA_Look"));
    IA_Look->ValueType = EInputActionValueType::Axis2D;

    // ── 建立 InputMappingContext ──────────────────────────────────────────
    DefaultIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));
    UInputMappingContext* IMC = DefaultIMC;

    // WASD：W=+Y(前), S=-Y(後), D=+X(右), A=-X(左)
    {   // W → FVector2D(0,+1)
        FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Move, EKeys::W);
        UInputModifierSwizzleAxis* Sw = NewObject<UInputModifierSwizzleAxis>(IMC);
        Sw->Order = EInputAxisSwizzle::YXZ;
        M.Modifiers.Add(Sw);
    }
    {   // S → FVector2D(0,-1)
        FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Move, EKeys::S);
        UInputModifierSwizzleAxis* Sw = NewObject<UInputModifierSwizzleAxis>(IMC);
        Sw->Order = EInputAxisSwizzle::YXZ;
        M.Modifiers.Add(Sw);
        UInputModifierNegate* Neg = NewObject<UInputModifierNegate>(IMC);
        M.Modifiers.Add(Neg);
    }
    IMC->MapKey(IA_Move, EKeys::D);  // D → FVector2D(+1,0)
    {   // A → FVector2D(-1,0)
        FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Move, EKeys::A);
        UInputModifierNegate* Neg = NewObject<UInputModifierNegate>(IMC);
        M.Modifiers.Add(Neg);
    }

    // 滑鼠視角：MouseX→X, MouseY→Y
    IMC->MapKey(IA_Look, EKeys::MouseX);
    {
        // 2026-06-22 修正上下反向：原始 EKeys::MouseY 滑鼠往上移動時數值是負的（螢幕座標
        // Y 往下為正），但 AddControllerPitchInput()（PlayerController.h:1567 doc comment
        // 寫明「正值 = look up」）要正值才會往上看，沒加 Negate 會變成滑鼠往上、鏡頭往下，
        // 使用者實測回報「游標上下相反」。
        FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Look, EKeys::MouseY);
        UInputModifierSwizzleAxis* Sw = NewObject<UInputModifierSwizzleAxis>(IMC);
        Sw->Order = EInputAxisSwizzle::YXZ;
        M.Modifiers.Add(Sw);
        UInputModifierNegate* Neg = NewObject<UInputModifierNegate>(IMC);
        M.Modifiers.Add(Neg);
    }

    // 其他按鍵
    IMC->MapKey(IA_Jump,      EKeys::SpaceBar);

    // 左鍵 Tap/Hold 雙軸分派（docs/plan-item-crafting-system.md §衝突3）：同一顆鍵綁兩個
    // Action，靠不同 Trigger 互斥——長按 0.2s 後 IA_Mine 才開始連續觸發（採礦），
    // 0.2s 內放開則 IA_PrimaryTap 觸發一次（攻擊/道具），兩者實務上不會同時生效。
    {
        FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Mine, EKeys::LeftMouseButton);
        UInputTriggerHold* Hold = NewObject<UInputTriggerHold>(IMC);
        Hold->HoldTimeThreshold = 0.2f;
        Hold->bIsOneShot = false; // 門檻後持續觸發，採礦進度才能每幀累加
        M.Triggers.Add(Hold);
    }
    {
        FEnhancedActionKeyMapping& M = IMC->MapKey(IA_PrimaryTap, EKeys::LeftMouseButton);
        UInputTriggerTap* Tap = NewObject<UInputTriggerTap>(IMC);
        Tap->TapReleaseTimeThreshold = 0.2f;
        M.Triggers.Add(Tap);
    }

    IMC->MapKey(IA_Place,     EKeys::RightMouseButton);
    IMC->MapKey(IA_SpellU,    EKeys::U);
    IMC->MapKey(IA_SpellI,    EKeys::I);
    IMC->MapKey(IA_SpellO,    EKeys::O);
    IMC->MapKey(IA_SpellP,    EKeys::P);
    // Ctrl 短按（≤0.2s）→ 視角切換；長按（Ctrl+滾輪縮放）不觸發，避免與縮放衝突
    {
        FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Camera, EKeys::LeftControl);
        UInputTriggerTap* Tap = NewObject<UInputTriggerTap>(IMC);
        Tap->TapReleaseTimeThreshold = 0.2f;
        M.Triggers.Add(Tap);
    }
    IMC->MapKey(IA_DbgTrace,  EKeys::F3);
    IMC->MapKey(IA_SnapTake,  EKeys::F5);
    IMC->MapKey(IA_SnapApply, EKeys::F6);
    IMC->MapKey(IA_DbgPaint,    EKeys::F1);
    IMC->MapKey(IA_DbgCoord,    EKeys::F2);
    IMC->MapKey(IA_DbgSurvival, EKeys::F4);

    // ── 注冊 MappingContext ───────────────────────────────────────────────
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
        if (auto* Sub = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            Sub->AddMappingContext(IMC, 0);

    // ── 綁定 Actions ─────────────────────────────────────────────────────
    EIC->BindAction(IA_Move, ETriggerEvent::Triggered,  this, &ASkillCreatorCharacter::Move);
    EIC->BindAction(IA_Look, ETriggerEvent::Triggered,  this, &ASkillCreatorCharacter::Look);
    // Space 長按大跳：短按=普通跳，長按≥0.1s 放開後追加向上衝量（plan-player-actions.md §B）
    EIC->BindAction(IA_Jump,  ETriggerEvent::Started,    this, &ASkillCreatorCharacter::OnJumpStarted);
    EIC->BindAction(IA_Jump,  ETriggerEvent::Completed,  this, &ASkillCreatorCharacter::OnJumpReleased);
    EIC->BindAction(IA_Mine,  ETriggerEvent::Triggered,  this, &ASkillCreatorCharacter::OnMine);
    EIC->BindAction(IA_Mine,  ETriggerEvent::Completed,  this, &ASkillCreatorCharacter::OnMineReleased);
    EIC->BindAction(IA_Mine,  ETriggerEvent::Canceled,   this, &ASkillCreatorCharacter::OnMineReleased);
    EIC->BindAction(IA_PrimaryTap, ETriggerEvent::Triggered, this, &ASkillCreatorCharacter::OnPrimaryTap);
    EIC->BindAction(IA_Place, ETriggerEvent::Triggered,  this, &ASkillCreatorCharacter::OnPlace);
    EIC->BindAction(IA_Place, ETriggerEvent::Completed,  this, &ASkillCreatorCharacter::OnPlaceReleased);
    EIC->BindAction(IA_Place, ETriggerEvent::Canceled,   this, &ASkillCreatorCharacter::OnPlaceReleased);

    EIC->BindAction(IA_SpellU, ETriggerEvent::Started, this, &ASkillCreatorCharacter::HandleSpellInput);
    EIC->BindAction(IA_SpellI, ETriggerEvent::Started, this, &ASkillCreatorCharacter::HandleSpellInput);
    EIC->BindAction(IA_SpellO, ETriggerEvent::Started, this, &ASkillCreatorCharacter::HandleSpellInput);
    EIC->BindAction(IA_SpellP, ETriggerEvent::Started, this, &ASkillCreatorCharacter::HandleSpellInput);

    EIC->BindAction(IA_Camera,    ETriggerEvent::Started, this, &ASkillCreatorCharacter::OnToggleCameraMode);
    EIC->BindAction(IA_DbgTrace,  ETriggerEvent::Started, this, &ASkillCreatorCharacter::OnDebugTrace);
    EIC->BindAction(IA_SnapTake,  ETriggerEvent::Started, this, &ASkillCreatorCharacter::OnDebugSnapshotTake);
    EIC->BindAction(IA_SnapApply, ETriggerEvent::Started, this, &ASkillCreatorCharacter::OnDebugSnapshotApply);
    EIC->BindAction(IA_DbgPaint,    ETriggerEvent::Started, this, &ASkillCreatorCharacter::OnDebugPaint);
    EIC->BindAction(IA_DbgCoord,    ETriggerEvent::Started, this, &ASkillCreatorCharacter::OnDebugCoord);
    EIC->BindAction(IA_DbgSurvival, ETriggerEvent::Started, this, &ASkillCreatorCharacter::OnDebugSurvival);
}

void ASkillCreatorCharacter::Move(const FInputActionValue& Value)
{
    if (!Controller) return;
    const FVector2D Axis = Value.Get<FVector2D>();

    if (IsFlying())
    {
        // 飛行模式：WASD 沿鏡頭朝向（含 Pitch）全向移動
        const FRotator FullRot = GetControlRotation();
        if (Axis.Y != 0.f) AddMovementInput(FRotationMatrix(FullRot).GetUnitAxis(EAxis::X), Axis.Y);
        if (Axis.X != 0.f) AddMovementInput(FRotationMatrix(FullRot).GetUnitAxis(EAxis::Y), Axis.X);
    }
    else
    {
        const FRotator YawOnly(0.f, GetControlRotation().Yaw, 0.f);
        if (Axis.Y != 0.f) AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X), Axis.Y);
        if (Axis.X != 0.f) AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y), Axis.X);
    }
}

void ASkillCreatorCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    AddControllerYawInput(Axis.X);
    AddControllerPitchInput(Axis.Y);
}

void ASkillCreatorCharacter::MoveForward(float Value)
{
    if (Value == 0.f || !Controller) return;
    const FRotator YawOnly(0, GetControlRotation().Yaw, 0);
    AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X), Value);
}

void ASkillCreatorCharacter::MoveRight(float Value)
{
    if (Value == 0.f || !Controller) return;
    const FRotator YawOnly(0, GetControlRotation().Yaw, 0);
    AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y), Value);
}

// ── Camera Mode ──────────────────────────────────────────────────────────

static FCameraModeParams DefaultParamsFor(ECameraMode Mode)
{
    FCameraModeParams P;
    switch (Mode)
    {
        case ECameraMode::ThirdPerson:
            P.ArmLength = 600.f; P.PitchDeg = -15.f; P.bUsePawnControlRotation = true;
            break;
        case ECameraMode::FirstPerson:
            P.ArmLength = 0.f;   P.PitchDeg = 0.f;   P.bUsePawnControlRotation = true;
            break;
        case ECameraMode::Isometric:
            P.ArmLength = 1200.f; P.PitchDeg = -55.f; P.bUsePawnControlRotation = false;
            P.FixedYawDeg = 45.f;
            // Godot CameraController.cs:164-171 ApplyProjection()：Isometric 用正交投影。
            P.bOrthographic = true; P.OrthoWidth = 900.f;
            break;
        case ECameraMode::SideScroll2D:
            // 對應 Godot CameraController.cs:219-226：相機在 -Z 側直接朝 +Z 看玩家，
            // Pitch=0（沒有俯角，純水平側視才是真 2D），加正交投影——2026-06-22 修正：
            // 原本 PitchDeg=-10 帶俯角、又是透視投影，兩個原因疊加讓 2D 視角看起來像
            // 「2.5D」（有透視深度線索 + 微俯角讓人看到地面延伸）。
            P.ArmLength = 900.f; P.PitchDeg = 0.f; P.bUsePawnControlRotation = false;
            P.FixedYawDeg = 90.f;
            P.bOrthographic = true; P.OrthoWidth = 900.f;
            break;
        default: break;
    }
    return P;
}

void ASkillCreatorCharacter::SetCameraMode(ECameraMode NewMode)
{
    CameraMode = NewMode;

    FCameraModeParams Params = DefaultParamsFor(NewMode);
    if (FCameraModeParams* P = CameraPresets.Find(NewMode))
        Params = *P;

    SpringArm->TargetArmLength           = Params.ArmLength;
    SpringArm->bUsePawnControlRotation   = Params.bUsePawnControlRotation;

    // 對應 Godot CameraController.cs:164-171 ApplyProjection()。
    Camera->SetProjectionMode(Params.bOrthographic
        ? ECameraProjectionMode::Orthographic
        : ECameraProjectionMode::Perspective);
    if (Params.bOrthographic)
        Camera->SetOrthoWidth(Params.OrthoWidth);

    if (NewMode == ECameraMode::FirstPerson)
    {
        // 一人稱：將 SpringArm 移到頭部位置（眼高 = 128cm ≒ 兩格）
        SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 128.f));
        SpringArm->SetRelativeRotation(FRotator::ZeroRotator);
    }
    else if (Params.bUsePawnControlRotation)
    {
        SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
        SpringArm->SetRelativeRotation(FRotator(Params.PitchDeg, 0.f, 0.f));
    }
    else
    {
        SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
        SpringArm->SetWorldRotation(FRotator(Params.PitchDeg, Params.FixedYawDeg, 0.f));
    }
}

void ASkillCreatorCharacter::CycleCameraMode()
{
    const ECameraMode Next = static_cast<ECameraMode>(
        (static_cast<uint8>(CameraMode) + 1) % 4);
    SetCameraMode(Next);
}

// ── Jump ────────────────────────────────────────────────────────────────

void ASkillCreatorCharacter::OnJumpStarted()
{
    JumpPressedTime = GetWorld()->GetTimeSeconds();

    // P-17: JumpFactor — 讀腳下材質的跳躍倍數（只在剛起跳時生效，空中二段跳不再重算）
    float TileJumpFactor = 1.0f;
    if (!GetCharacterMovement()->IsFalling() && CachedVoxelWorld)
    {
        if (FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld())
        {
            const FGridPos Pos = WorldScale::WorldToTile(GetActorLocation(), TW->Height);
            TileJumpFactor = FMaterialRegistry::Get(TW->GetTile(Pos.X, Pos.Y + 1, Pos.Z)).JumpFactor;
        }
    }

    if (GetCharacterMovement()->IsFalling() && JumpCount < MaxJumpCount)
    {
        // 二段跳：給予額外向上衝量（0.9x JumpZVelocity）
        ++JumpCount;
        LaunchCharacter(FVector(0.f, 0.f, GetCharacterMovement()->JumpZVelocity * 0.9f), false, true);
    }
    else if (!GetCharacterMovement()->IsFalling())
    {
        ++JumpCount;
        // 臨時縮放 JumpZVelocity，Jump() 讀取後立刻還原（不影響後續空中物理）
        const float OrigZ = GetCharacterMovement()->JumpZVelocity;
        GetCharacterMovement()->JumpZVelocity = OrigZ * TileJumpFactor;
        Jump();
        GetCharacterMovement()->JumpZVelocity = OrigZ;
    }
}

void ASkillCreatorCharacter::OnJumpReleased()
{
    StopJumping();
    const float HoldDuration = GetWorld()->GetTimeSeconds() - JumpPressedTime;
    // 0.1~0.5s 按住範圍：線性插值至最多 1.5x JumpZVelocity 額外衝量
    const float Alpha = FMath::Clamp((HoldDuration - 0.1f) / 0.4f, 0.f, 1.f);
    const float Bonus = Alpha * WorldScale::JumpZVelocityCm * 1.5f;
    if (Bonus > 0.f && GetMovementComponent()->IsFalling())
        LaunchCharacter(FVector(0.f, 0.f, Bonus), false, false);
}

void ASkillCreatorCharacter::Landed(const FHitResult& Hit)
{
    // P-16: Restitution — 在 Super 之前讀落下速度（Super 之後移動元件才清零 Z 速）
    const float FallSpeedZ = FMath::Abs(GetCharacterMovement()->Velocity.Z);

    Super::Landed(Hit);
    JumpCount = 0;
    if (MovementState == EPlayerMovementState::Flying) return; // 飛行中強制落地不改狀態
    if (MovementState == EPlayerMovementState::Rolling
     || MovementState == EPlayerMovementState::Sliding) return;
    if (!IsFlying() && MovementState != EPlayerMovementState::Grounded
                    && MovementState != EPlayerMovementState::Sprinting
                    && MovementState != EPlayerMovementState::SuperSprinting
                    && MovementState != EPlayerMovementState::Guarding
                    && MovementState != EPlayerMovementState::Crouching)
    {
        MovementState = EPlayerMovementState::Grounded;
        ApplyMovementState();
    }

    // P-16: Restitution — 腳下材質反彈（FallSpeedZ 在 Super 之前已讀取）
    if (FallSpeedZ > 1.f && CachedVoxelWorld)
    {
        if (FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld())
        {
            const FGridPos Pos = WorldScale::WorldToTile(GetActorLocation(), TW->Height);
            const float Restitution = FMaterialRegistry::Get(TW->GetTile(Pos.X, Pos.Y + 1, Pos.Z)).Restitution;
            if (Restitution > 0.f)
                LaunchCharacter(FVector(0.f, 0.f, FallSpeedZ * Restitution), false, true);
        }
    }
}

// ── S-1 移動狀態機 ────────────────────────────────────────────────────────

void ASkillCreatorCharacter::ApplyMovementState()
{
    // 離開蹲伏/滑鏟狀態時取消蹲伏
    const bool bNeedsCrouch = (MovementState == EPlayerMovementState::Crouching
                             || MovementState == EPlayerMovementState::Sliding);
    if (bIsCrouched && !bNeedsCrouch) UnCrouch();

    switch (MovementState)
    {
        case EPlayerMovementState::Grounded:
        case EPlayerMovementState::Guarding:
            GetCharacterMovement()->MaxWalkSpeed = WorldScale::WalkSpeedCm;
            break;
        case EPlayerMovementState::Sprinting:
            GetCharacterMovement()->MaxWalkSpeed = WorldScale::WalkSpeedCm * 2.f;
            break;
        case EPlayerMovementState::SuperSprinting:
            GetCharacterMovement()->MaxWalkSpeed = WorldScale::WalkSpeedCm * 4.f;
            break;
        case EPlayerMovementState::Flying:
            GetCharacterMovement()->MaxFlySpeed = WorldScale::WalkSpeedCm * 2.5f;
            break;
        case EPlayerMovementState::FastFlying:
            GetCharacterMovement()->MaxFlySpeed = WorldScale::WalkSpeedCm * 5.f;
            break;
        case EPlayerMovementState::Crouching:
            GetCharacterMovement()->MaxWalkSpeedCrouched = WorldScale::WalkSpeedCm * 0.5f;
            if (!bIsCrouched) Crouch();
            break;
        case EPlayerMovementState::Rolling:
            GetCharacterMovement()->MaxWalkSpeed = WorldScale::WalkSpeedCm * 1.5f;
            break;
        case EPlayerMovementState::Sliding:
            GetCharacterMovement()->MaxWalkSpeed = WorldScale::WalkSpeedCm * 2.f;
            if (!bIsCrouched) Crouch();
            break;
    }
}

void ASkillCreatorCharacter::StartSprint()
{
    if (IsFlying())
    {
        if (MovementState != EPlayerMovementState::FastFlying)
        {
            MovementState = EPlayerMovementState::FastFlying;
            ApplyMovementState();
        }
    }
    else if (MovementState == EPlayerMovementState::Grounded
          || MovementState == EPlayerMovementState::Crouching)
    {
        if (IsCrouching()) EndCrouch();
        MovementState = EPlayerMovementState::Sprinting;
        ApplyMovementState();
        // 長按 1 秒後升級至超速
        GetWorldTimerManager().SetTimer(SprintTimer, FTimerDelegate::CreateLambda([this]()
        {
            if (MovementState == EPlayerMovementState::Sprinting)
            {
                MovementState = EPlayerMovementState::SuperSprinting;
                ApplyMovementState();
            }
        }), 1.f, false);
    }
}

void ASkillCreatorCharacter::EndSprint()
{
    GetWorldTimerManager().ClearTimer(SprintTimer);
    if (MovementState == EPlayerMovementState::Sprinting
     || MovementState == EPlayerMovementState::SuperSprinting)
    {
        MovementState = EPlayerMovementState::Grounded;
        ApplyMovementState();
    }
    else if (MovementState == EPlayerMovementState::FastFlying)
    {
        MovementState = EPlayerMovementState::Flying;
        ApplyMovementState();
    }
}

void ASkillCreatorCharacter::ToggleFlight()
{
    if (IsFlying())
    {
        MovementState = EPlayerMovementState::Grounded;
        GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        GetCharacterMovement()->GravityScale = WorldScale::GravityScaleMult;
        ApplyMovementState();
    }
    else if (GetCharacterMovement()->IsFalling())
    {
        MovementState = EPlayerMovementState::Flying;
        GetCharacterMovement()->SetMovementMode(MOVE_Flying);
        GetCharacterMovement()->GravityScale = 0.f;
        ApplyMovementState();
    }
}

void ASkillCreatorCharacter::FlyDown()
{
    if (!IsFlying()) return;
    MovementState = EPlayerMovementState::Grounded;
    GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    GetCharacterMovement()->GravityScale = WorldScale::GravityScaleMult;
    // 向下衝量 = 3x WalkSpeed（讓玩家快速落地）
    LaunchCharacter(FVector(0.f, 0.f, -WorldScale::WalkSpeedCm * 3.f), false, true);
}

// ── S-3 鎖敵 ─────────────────────────────────────────────────────────────

bool ASkillCreatorCharacter::TryToggleLockTarget()
{
    if (LockedTarget) { LockedTarget = nullptr; return true; }
    if (!CachedEnemyMgr) return false;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return false;

    int32 VX = 0, VY = 0;
    PC->GetViewportSize(VX, VY);
    const FVector2D ScreenCenter(VX * 0.5f, VY * 0.5f);
    const float MaxDistCm = WorldScale::TileSizeCm * WorldScale::GrainCurrent * 25.f; // 25 遊戲單位

    AEnemy* Best     = nullptr;
    float   BestScore = FLT_MAX;

    for (AEnemy* E : CachedEnemyMgr->GetEnemies())
    {
        if (!E || !E->IsAlive()) continue;
        const float Dist = FVector::Dist(GetActorLocation(), E->GetActorLocation());
        if (Dist > MaxDistCm) continue;

        FVector2D SP;
        if (!PC->ProjectWorldLocationToScreen(E->GetActorLocation(), SP)) continue;

        // 距離分數 + 螢幕中心偏移分數（各佔一半）
        const float Score = Dist / MaxDistCm + FVector2D::Distance(SP, ScreenCenter) / 500.f;
        if (Score < BestScore) { BestScore = Score; Best = E; }
    }

    LockedTarget = Best;
    return Best != nullptr;
}

void ASkillCreatorCharacter::SwitchToNextLockTarget()
{
    if (!CachedEnemyMgr) return;
    const float MaxDistCm = WorldScale::TileSizeCm * WorldScale::GrainCurrent * 25.f;

    TArray<AEnemy*> Valid;
    for (AEnemy* E : CachedEnemyMgr->GetEnemies())
    {
        if (E && E->IsAlive() && FVector::Dist(GetActorLocation(), E->GetActorLocation()) <= MaxDistCm)
            Valid.Add(E);
    }
    if (Valid.IsEmpty()) { LockedTarget = nullptr; return; }

    const int32 Cur = Valid.IndexOfByKey(LockedTarget.Get());
    LockedTarget = Valid[(Cur + 1) % Valid.Num()];
}

// ── S-2 攻擊框架（骨架）──────────────────────────────────────────────────

void ASkillCreatorCharacter::PerformLightAttack()
{
    if (!IsAlive() || !CachedEnemyMgr) return;

    // 傷害基底：20 × 熱鍵欄武器 AtkMult（對應 Godot SpellCaster.cs:128 meleeDmg = 20f * AtkMult）
    const FItemStack& ActiveWpn = InventoryComp ? InventoryComp->GetActiveItem() : FItemStack{};
    const bool bIsWeapon  = !ActiveWpn.IsEmpty() && FItemRegistry::Get(ActiveWpn.ItemId).bIsWeapon;
    const float AtkMult   = bIsWeapon ? FItemRegistry::Get(ActiveWpn.ItemId).AtkMult : 1.f;
    const float PhysAtk   = FMath::Max(20.f * AtkMult, Stats.Strength);
    const float GameUnit = WorldScale::TileSizeCm * WorldScale::GrainCurrent;
    const FVector Center = GetActorLocation() + GetActorForwardVector() * GameUnit * 1.5f;

    auto* GI  = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    auto* Sub = GI ? GI->GetSubsystem<UCombatStateSubsystem>() : nullptr;

    for (AEnemy* E : CachedEnemyMgr->GetEnemies())
    {
        if (!E || !E->IsAlive()) continue;
        if (FVector::Dist(Center, E->GetActorLocation()) <= GameUnit)
        {
            E->TakePhysicalDamage(PhysAtk, &Stats);
            if (Sub) Sub->OnPlayerDealtDamage(PhysAtk);
            OnAttackHit.Broadcast(EAttackType::Light, E);
        }
    }
}

// ── Panel / Debug Key Handlers ──────────────────────────────────────────

void ASkillCreatorCharacter::OnToggleCameraMode()
{
    CycleCameraMode();
}

void ASkillCreatorCharacter::OnDebugTrace()
{
    FExecutionContext::bTraceMode = !FExecutionContext::bTraceMode;
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow,
            FString::Printf(TEXT("[F3] TraceMode: %s"),
                FExecutionContext::bTraceMode ? TEXT("ON") : TEXT("OFF")));
}

void ASkillCreatorCharacter::OnDebugSnapshotTake()
{
    auto* GI   = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    auto* Snap = GI ? GI->GetSubsystem<USnapshotManager>() : nullptr;
    if (!Snap) return;

    static const TArray<AEnemy*> NoEnemies;
    const TArray<AEnemy*>& Enemies = CachedEnemyMgr ? CachedEnemyMgr->GetEnemies() : NoEnemies;
    Snap->TakeSnapshot(this, Enemies, CachedVoxelWorld);
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green,
            FString::Printf(TEXT("[F5] 快照已儲存（堆疊深度 %d）"), Snap->StackDepth()));
}

void ASkillCreatorCharacter::OnDebugSnapshotApply()
{
    auto* GI   = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    auto* Snap = GI ? GI->GetSubsystem<USnapshotManager>() : nullptr;
    if (!Snap) return;

    static const TArray<AEnemy*> NoEnemies;
    const TArray<AEnemy*>& Enemies = CachedEnemyMgr ? CachedEnemyMgr->GetEnemies() : NoEnemies;
    bool bOk = Snap->ApplyLatest(this, Enemies, CachedVoxelWorld);
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 3.f, bOk ? FColor::Green : FColor::Red,
            bOk ? TEXT("[F6] 快照已還原") : TEXT("[F6] 無快照可還原"));
}

// ── F1/F2/F4 開發者工具（對應 Godot TogglePaint/DebugCoord/DebugSurvival）─────

void ASkillCreatorCharacter::OnDebugPaint()
{
    bDebugPaintEnabled = !bDebugPaintEnabled;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (ASkillCreatorHUD* HUD = PC ? Cast<ASkillCreatorHUD>(PC->GetHUD()) : nullptr)
        HUD->ToggleDebugPaint();

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow,
            bDebugPaintEnabled
                ? TEXT("[F1] 畫筆模式 ON（左鍵塗材質 / 右鍵清除）")
                : TEXT("[F1] 畫筆模式 OFF"));
}

void ASkillCreatorCharacter::OnDebugCoord()
{
    bDebugCoordEnabled = !bDebugCoordEnabled;
    if (!bDebugCoordEnabled && GEngine)
        GEngine->RemoveOnScreenDebugMessage(9901);
}

void ASkillCreatorCharacter::OnDebugSurvival()
{
    bDebugSurvivalEnabled = !bDebugSurvivalEnabled;
    if (!bDebugSurvivalEnabled && GEngine)
        GEngine->RemoveOnScreenDebugMessage(9902);
}

// ── Spell Input ─────────────────────────────────────────────────────────

void ASkillCreatorCharacter::HandleSpellInput()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    // J+U 重攻擊：U 觸發時若 J 已蓄力 ≥ HeavyAttackMinCharge → 重攻（阻斷施法）
    if (PC->IsInputKeyDown(EKeys::U) && bChargingAttack && AttackChargeTimer >= HeavyAttackMinCharge)
    {
        PerformHeavyAttack();
        return;
    }

    if (!SpellCasterComp) return;

    const bool bU = PC->IsInputKeyDown(EKeys::U);
    const bool bI = PC->IsInputKeyDown(EKeys::I);
    const bool bO = PC->IsInputKeyDown(EKeys::O);
    const bool bP = PC->IsInputKeyDown(EKeys::P);

    // Longest combination takes priority (matches Godot InputBindings logic)
    int32 Slot = -1;
    if      (bU && bI && bO && bP) Slot = 9;
    else if (bI && bO && bP)       Slot = 8;
    else if (bU && bI && bO)       Slot = 7;
    else if (bO && bP)             Slot = 6;
    else if (bI && bO)             Slot = 5;
    else if (bU && bI)             Slot = 4;
    else if (bP)                   Slot = 3;
    else if (bO)                   Slot = 2;
    else if (bI)                   Slot = 1;
    else if (bU)                   Slot = 0;

    if (Slot >= 0)
        SpellCasterComp->TryCastSlot(Slot);
}

// ── Mining / Placement ──────────────────────────────────────────────────
// 對應 Godot Main.cs:1197-1287（採掘/放置主迴圈）+ PlayerController.cs:430-471（TickMining）。

void ASkillCreatorCharacter::OnMine()
{
    // 持有道具時，長按也是使用道具（覆寫採礦），docs/plan-item-crafting-system.md §衝突3。
    // rising-edge 防止長按期間每幀重複使用（同 bRightMouseWasPressed 同款防重複機制）。
    if (InventoryComp)
    {
        const FItemStack& Active = InventoryComp->GetActiveItem();
        if (!Active.IsEmpty() && FItemRegistry::Get(Active.ItemId).bIsConsumable)
        {
            if (!bMineWasPressed) { bMineWasPressed = true; UseConsumable(); }
            return;
        }
    }

    if (!CachedVoxelWorld) { CancelMining(); CancelCollect(); return; }
    FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld();
    if (!TW) { CancelMining(); CancelCollect(); return; }
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) { CancelMining(); CancelCollect(); return; }

    // W-D：採集分流 — 在 tile raycast 之前先找最近的 ICollectible Entity
    // （範圍 = WorldScale::MiningRangeTiles tiles 球形 overlap；Entity 優先於 tile）
    {
        const float CollectRangeCm = WorldScale::MiningRangeTiles * WorldScale::TileSizeCm;
        TArray<FOverlapResult> Overlaps;
        FCollisionQueryParams QParams;
        QParams.AddIgnoredActor(this);
        GetWorld()->OverlapMultiByChannel(Overlaps,
            GetActorLocation(), FQuat::Identity,
            ECC_WorldDynamic,
            FCollisionShape::MakeSphere(CollectRangeCm),
            QParams);

        AActor* NearestCollectible = nullptr;
        float   NearestDist        = FLT_MAX;
        for (const FOverlapResult& R : Overlaps)
        {
            AActor* A = R.GetActor();
            if (!A || !A->Implements<UCollectible>()) continue;
            if (!Cast<ICollectible>(A)->CanBeCollected(PC)) continue;
            const float D = FVector::Dist(GetActorLocation(), A->GetActorLocation());
            if (D < NearestDist) { NearestDist = D; NearestCollectible = A; }
        }

        if (NearestCollectible)
        {
            CancelMining();
            if (CollectTarget.Get() != NearestCollectible)
            {
                CollectTarget   = NearestCollectible;
                CollectProgress = 0.f;
            }
            CollectProgress += GetWorld()->GetDeltaSeconds();
            if (CollectProgress >= CollectFixedTime)
            {
                Cast<ICollectible>(NearestCollectible)->Collect(PC);
                CancelCollect();
            }
            return;
        }
        CancelCollect();
    }

    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);
    const FVector CamDir = CamRot.Vector();

    const FVector TileStart = WorldScale::WorldToTileF(CamLoc, TW->Height);
    const FVector VoxDir    = WorldScale::DirToVoxel(CamDir);

    // Godot Main.cs:1012 Raycast 最遠 50 game-units；UE5 tile 換算後應至少到 MiningRangeTiles。
    // 舊值 10.f tiles = 62.5cm，遠小於 MiningRangeTiles(192 tiles=12m)，玩家完全採不到任何方塊。
    FRaycastResult3D Hit = TW->Raycast(TileStart, VoxDir, static_cast<float>(WorldScale::MiningRangeTiles));
    if (!Hit.bHit) { CancelMining(); return; }

    // 採掘距離限制（Godot PlayerController.cs:42 MiningRange => BodyH * 6f）
    const FGridPos PlayerPos = GetPosition();
    const FGridPos TargetPos(Hit.HitCell.X, Hit.HitCell.Y, Hit.HitCell.Z);
    if (PlayerPos.EuclideanDistance(TargetPos) > static_cast<float>(WorldScale::MiningRangeTiles))
    {
        CancelMining();
        return;
    }

    // K-12：F1 畫筆模式——左鍵立即塗 HUD->ActivePaintMaterial（取代採掘），不消耗
    // MiningProgress/工具判定。對應 Godot Main.cs:569-583 材質按鈕意圖（Godot 本身
    // 該處理器只是 GD.Print stub，UE5 版本改為真正落地塗繪）。
    if (bDebugPaintEnabled)
    {
        if (ASkillCreatorHUD* HUD = Cast<ASkillCreatorHUD>(PC->GetHUD()))
        {
            for (const FIntVector& Off : FPlacementShape::GetOffsets(EPlacementShape::Cube, HUD->PaintBrushRadius))
                TW->SetTile(Hit.HitCell.X + Off.X, Hit.HitCell.Y + Off.Y, Hit.HitCell.Z + Off.Z,
                            HUD->ActivePaintMaterial);
        }
        return;
    }

    // ── 漸進式採掘（Godot PlayerController.cs:442-471 TickMining）─────────
    const EMaterialType TargetMat = TW->GetTile(Hit.HitCell.X, Hit.HitCell.Y, Hit.HitCell.Z);
    const FMaterialData& MatData  = FMaterialRegistry::Get(TargetMat);

    const int32 ActiveToolTier = InventoryComp ? InventoryComp->GetActiveToolTier() : 0;
    if (!MatData.bIsMineable || MatData.RequiredToolTier > ActiveToolTier)
    {
        CancelMining();
        return;
    }

    // 換目標 → 重置進度
    if (!MiningTarget.IsSet() || MiningTarget.GetValue() != Hit.HitCell)
    {
        MiningTarget   = Hit.HitCell;
        MiningProgress = 0.f;
    }

    // 以 60fps 為基準累加進度，乘上工具速度倍率（Godot PlayerController.cs:461）
    const float SpeedMult = InventoryComp ? InventoryComp->GetActiveMiningSpeedMult(TargetMat) : 1.f;
    MiningProgress += SpeedMult * GetWorld()->GetDeltaSeconds() * 60.f;

    if (MiningProgress < MatData.Hardness)
        return; // 仍在進行中，中心格尚未被摧毀

    CancelMining();

    // 讀取 HUD 的形狀/完美移除設定（N 鍵 ShapeMenuWidget 設定；預設 Cube + WorldScale::DefaultShapeRadius，
    // 對應 Godot Main.cs:71-72 _activeShape=Cube / _shapeRadius=PlayerH/6）
    EPlacementShape Shape = EPlacementShape::Cube;
    int32 Radius = WorldScale::DefaultShapeRadius;
    bool  bPerfectRemove = true;
    if (ASkillCreatorHUD* HUD = Cast<ASkillCreatorHUD>(PC->GetHUD()))
    {
        Shape  = HUD->ActiveShape;
        Radius = HUD->PlaceRadius;
        bPerfectRemove = HUD->bPerfectRemove;
    }

    // K-5：完美移除分流（對應 Main.cs:1203-1225）。採掘前先查目標格是否屬於某個
    // PlacedUnit；若是且開啟完美移除，整個 Unit 的剩餘格子一起摧毀，不做形狀採掘。
    FPlacedObjectRegistry& Registry = CachedVoxelWorld->GetPlacedRegistry();
    FPlacedUnit* HitUnit = bPerfectRemove ? Registry.FindAtTile(Hit.HitCell) : nullptr;

    if (HitUnit)
    {
        TArray<FIntVector> Remaining = HitUnit->Tiles.Array();
        const int32 UnitId = HitUnit->PlacedUnitId;
        Registry.Unregister(UnitId);  // 對應 Main.cs:1218 RemoveUnit（先於摧毀，避免 NotifyDestroyed 競爭）
        for (const FIntVector& P : Remaining)
            if (TW->GetTile(P.X, P.Y, P.Z) != EMaterialType::Air)
                TW->DestroyTile(P.X, P.Y, P.Z, EDestroyReason::ShapeMining);
    }
    else
    {
        // 一般形狀採掘路線（世界原生 tile 或完美移除關閉；對應 Main.cs:1228-1235）
        TW->DestroyTile(Hit.HitCell.X, Hit.HitCell.Y, Hit.HitCell.Z, EDestroyReason::ShapeMining);
        for (const FIntVector& Off : FPlacementShape::GetOffsets(Shape, Radius))
        {
            if (Off == FIntVector::ZeroValue) continue;
            const int32 tx = Hit.HitCell.X + Off.X;
            const int32 ty = Hit.HitCell.Y + Off.Y;
            const int32 tz = Hit.HitCell.Z + Off.Z;
            if (TW->GetTile(tx, ty, tz) != EMaterialType::Air)
                TW->DestroyTile(tx, ty, tz, EDestroyReason::ShapeMining);
        }
    }

    // 整個形狀採掘只 spawn 1 個掉落物（對應 Main.cs:1238；ShapeMining 在
    // UDroppedItemManager::SpawnForReason 已不再 per-tile 生成，這裡顯式呼叫一次）
    if (UWorld* W = GetWorld())
        if (UDroppedItemManager* DropMgr = W->GetSubsystem<UDroppedItemManager>())
            DropMgr->SpawnForReason(Hit.HitCell.X, Hit.HitCell.Y, Hit.HitCell.Z, TargetMat, EDestroyReason::Mining);
}

// 對應 Godot Main.cs:2935-2968 ComputeHighlightTiles()：算出這次按下去真正會被摧毀的
// 整個 tile 集合，過濾條件跟 OnMine() 完全對齊（不可挖/工具等級不足/超出範圍/滑鼠在
// 熱鍵欄上一律不顯示），不是隨便 raycast 打到什麼就顯示什麼。
TArray<FGridPos> ASkillCreatorCharacter::ComputeHighlightTiles() const
{
    if (!CachedVoxelWorld) return {};
    FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld();
    if (!TW) return {};
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return {};

    FVector CamLoc; FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);
    FRaycastResult3D Hit = TW->Raycast(
        WorldScale::WorldToTileF(CamLoc, TW->Height),
        WorldScale::DirToVoxel(CamRot.Vector()),
        static_cast<float>(WorldScale::MiningRangeTiles));
    if (!Hit.bHit) return {};

    const FGridPos Target(Hit.HitCell.X, Hit.HitCell.Y, Hit.HitCell.Z);
    if (GetPosition().EuclideanDistance(Target) > static_cast<float>(WorldScale::MiningRangeTiles))
        return {};

    const EMaterialType TargetMat = TW->GetTile(Target.X, Target.Y, Target.Z);
    const FMaterialData& MatData  = FMaterialRegistry::Get(TargetMat);
    if (!MatData.bIsMineable) return {};

    const int32 ActiveToolTier = InventoryComp ? InventoryComp->GetActiveToolTier() : 0;
    if (MatData.RequiredToolTier > ActiveToolTier) return {};

    EPlacementShape Shape = EPlacementShape::Cube;
    int32 Radius = WorldScale::DefaultShapeRadius;
    bool  bPerfectRemove = true;
    if (const ASkillCreatorHUD* HUD = PC->GetHUD<ASkillCreatorHUD>())
    {
        Shape  = HUD->ActiveShape;
        Radius = HUD->PlaceRadius;
        bPerfectRemove = HUD->bPerfectRemove;
    }

    // 完美移除模式：命中已放置物件 → 整個 Unit 一起高亮（對應 Main.cs:2948-2956）
    if (bPerfectRemove)
    {
        FPlacedObjectRegistry& Registry = CachedVoxelWorld->GetPlacedRegistry();
        if (FPlacedUnit* Unit = Registry.FindAtTile(FIntVector(Target.X, Target.Y, Target.Z)))
        {
            TArray<FGridPos> Out;
            for (const FIntVector& P : Unit->Tiles)
                if (TW->GetTile(P.X, P.Y, P.Z) != EMaterialType::Air)
                    Out.Add(FGridPos(P.X, P.Y, P.Z));
            if (Out.Num() > 0)
            {
                // Target 永遠排第一個（AVoxelWorldActor::ShowHighlight 用 [0] 當中心格）
                Out.RemoveSingleSwap(Target);
                Out.Insert(Target, 0);
                return Out;
            }
        }
    }

    // 一般形狀採掘：中心格 + 形狀偏移內所有非空格（對應 Main.cs:2958-2967）
    TArray<FGridPos> Out;
    Out.Add(Target);
    for (const FIntVector& Off : FPlacementShape::GetOffsets(Shape, Radius))
    {
        if (Off == FIntVector::ZeroValue) continue;
        const FGridPos P(Target.X + Off.X, Target.Y + Off.Y, Target.Z + Off.Z);
        if (TW->GetTile(P.X, P.Y, P.Z) != EMaterialType::Air)
            Out.Add(P);
    }
    return Out;
}

// 互動系統（docs/plan-item-crafting-system.md §五）：每幀對準心方向做 LineTrace，
// 找最近實作 IInteractable 的 Actor。跟 tile raycast（TW->Raycast）是兩條平行的判定
// 路徑：這個是 UE5 物理查詢，找的是寶箱/工作臺這類獨立 Actor，不是 tile。
void ASkillCreatorCharacter::UpdateInteractableTrace()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) { CachedInteractable = nullptr; return; }

    FVector CamLoc; FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);
    const float RangeCm = static_cast<float>(WorldScale::InteractRangeTiles) * WorldScale::TileSizeCm;

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractTrace), false, this);
    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit, CamLoc, CamLoc + CamRot.Vector() * RangeCm, ECC_Visibility, Params);

    AActor* Found = nullptr;
    if (bHit && Hit.GetActor() && Hit.GetActor()->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
        Found = Hit.GetActor();

    CachedInteractable = Found;
    if (Found)
    {
        const FBox Bounds = Found->GetComponentsBoundingBox();
        DrawDebugBox(GetWorld(), Bounds.GetCenter(), Bounds.GetExtent(), FQuat::Identity,
                     FColor::Green, false, 0.05f, 0, 1.5f);
    }
}

void ASkillCreatorCharacter::OnPlace()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    // 互動優先判定（docs/plan-item-crafting-system.md §衝突1）：準心對著的是
    // IInteractable Actor（寶箱/工作臺）時，右鍵＝互動，不繼續走放置流程。
    if (AActor* Interactable = CachedInteractable.Get())
    {
        IInteractable::Execute_Interact(Interactable, this);
        return;
    }

    if (!CachedVoxelWorld || !InventoryComp) return;
    FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld();
    if (!TW) return;

    // 放置節流：_holdToPlace=true 時 0.12 秒節流連放，false 時 rising-edge
    // （對應 Main.cs:1250-1255）。HUD->bHoldToPlace 預設 false（rising-edge），
    // Enhanced Input 的 Triggered 事件每幀觸發，故節流邏輯需要角色自己持有計時器。
    ASkillCreatorHUD* HUD = Cast<ASkillCreatorHUD>(PC->GetHUD());
    const bool bHoldToPlace = HUD && HUD->bHoldToPlace;

    if (bHoldToPlace)
    {
        if (PlaceCooldown > 0.f) return;
    }
    else
    {
        if (bRightMouseWasPressed) return; // 按住不放：rising-edge 已觸發過，等鬆開再按
        bRightMouseWasPressed = true;
    }

    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    const FVector TileStart = WorldScale::WorldToTileF(CamLoc, TW->Height);
    const FVector VoxDir    = WorldScale::DirToVoxel(CamRot.Vector());

    FRaycastResult3D Hit = TW->Raycast(TileStart, VoxDir, 10.f);
    if (!Hit.bHit) return;

    // K-12：F1 畫筆模式——右鍵清除（設為 Air），不消耗物品/不走放置驗證
    if (bDebugPaintEnabled)
    {
        if (HUD)
            for (const FIntVector& Off : FPlacementShape::GetOffsets(EPlacementShape::Cube, HUD->PaintBrushRadius))
                TW->SetTile(Hit.HitCell.X + Off.X, Hit.HitCell.Y + Off.Y, Hit.HitCell.Z + Off.Z,
                            EMaterialType::Air);
        return;
    }

    // 取熱鍵格目前選中的物品
    const int32 Idx = InventoryComp->ActiveHotbarIndex;
    if (!InventoryComp->Slots.IsValidIndex(Idx)) return;
    const FItemStack& Stack = InventoryComp->Slots[Idx];
    if (Stack.IsEmpty()) return;

    const FItemData& Data = FItemRegistry::Get(Stack.ItemId);
    if (!Data.bIsPlaceable) return;
    if (Data.PlaceAs == EMaterialType::Air && Data.PlaceAsActor == nullptr) return;

    // 放置中心 = 命中面法線方向的相鄰格（對應 Main.cs:1262 MouseGridPos + MouseFaceNormal）
    const FIntVector PlaceCenter = Hit.HitCell + Hit.FaceNormal;
    const FGridPos PlayerPos = GetPosition();
    FPlacedObjectRegistry& Registry = CachedVoxelWorld->GetPlacedRegistry();

    // 不可塑形可放置物（寶箱/工作臺等，docs/plan-item-crafting-system.md §四、六、七）：
    // 固定單一 Actor，不吃形狀選單，永遠只放一格。
    if (Data.PlaceAsActor != nullptr)
    {
        const FGridPos CenterGrid(PlaceCenter.X, PlaceCenter.Y, PlaceCenter.Z);
        if (IsTileOccupiedByEntity(PlaceCenter)) return;
        if (PlayerPos.EuclideanDistance(CenterGrid) > static_cast<float>(WorldScale::MiningRangeTiles)) return;
        if (!FPlacementValidator::CanPlace(*TW, { PlaceCenter }, EMaterialType::Fixture, &Registry)) return;

        const FVector SpawnLoc = WorldScale::TileToWorld(CenterGrid, TW->Height);
        if (GetWorld()->SpawnActor<AActor>(Data.PlaceAsActor, FTransform(SpawnLoc)))
        {
            // Fixture：純粹標記占用，不可採、不影響 Greedy Meshing（Actor 自己有 mesh）
            TW->SetTile(PlaceCenter.X, PlaceCenter.Y, PlaceCenter.Z, EMaterialType::Fixture);

            FPlacedUnit FixtureUnit;
            FixtureUnit.Material = EMaterialType::Fixture;
            FixtureUnit.Tiles.Add(PlaceCenter);
            Registry.Register(FixtureUnit);

            InventoryComp->Consume(Idx, 1);
            PlaceCooldown = 0.12f;
        }
        return;
    }

    EPlacementShape Shape = EPlacementShape::Cube;
    int32 Radius = WorldScale::DefaultShapeRadius;
    if (HUD)
    {
        Shape  = HUD->ActiveShape;
        Radius = HUD->PlaceRadius;
    }

    TArray<FIntVector> PlacedTiles;
    for (const FIntVector& Off : FPlacementShape::GetOffsets(Shape, Radius))
    {
        const FIntVector P = PlaceCenter + Off;
        const FGridPos PGrid(P.X, P.Y, P.Z);

        // 玩家/敵人佔用格不可放置（對應 Main.cs:1270 !OccupiedByEntity(p)）
        if (IsTileOccupiedByEntity(P))
            continue;

        // 每格距離限制（對應 Main.cs:1271 _player.Position.DistanceTo(p) <= MiningRange）
        if (PlayerPos.EuclideanDistance(PGrid) > static_cast<float>(WorldScale::MiningRangeTiles))
            continue;

        // 每格放置合法性（對應 Main.cs:1272 PlacementValidator.CanPlace；K-5：補上 Registry
        // 避免疊放到既有 PlacedUnit 上）
        if (!FPlacementValidator::CanPlace(*TW, { P }, Data.PlaceAs, &Registry))
            continue;

        TW->SetTile(P.X, P.Y, P.Z, Data.PlaceAs);
        PlacedTiles.Add(P);
    }

    if (PlacedTiles.Num() > 0)
    {
        // K-5：登記為一個 PlacedUnit，供之後「完美移除」整體判定（對應 Main.cs:1280）
        FPlacedUnit Unit;
        Unit.Material = Data.PlaceAs;
        Unit.Tiles.Append(PlacedTiles);
        Registry.Register(Unit);

        // 只消耗 1 個物品，不管形狀放了多少格（對應 Main.cs:1281）
        InventoryComp->Consume(Idx, 1);
        PlaceCooldown = 0.12f; // 對應 Main.cs:1282
    }
}

// 對應 Godot Main.cs:1654 OccupiedByEntity（玩家/敵人佔用格不可放置）。
// 玩家檢查刻意補上 Z 比對：Godot 該函式只比 X/Y（2D→3D 遷移殘留，Main.cs:1656-1658
// 沒有 e.Position.Z 那種精確比對，會把同 X/Y 不同 Z 層的格子誤判為玩家佔用）；
// UE5 世界原生 3D，沿用會在多層地形放置時誤擋鄰層格子，故補 Pos.Z == PP.Z。
bool ASkillCreatorCharacter::IsTileOccupiedByEntity(const FIntVector& Pos) const
{
    const FGridPos PP = GetPosition();
    if (Pos.X >= PP.X && Pos.X < PP.X + WorldScale::PlayerW &&
        Pos.Y >= PP.Y && Pos.Y < PP.Y + WorldScale::PlayerH &&
        Pos.Z == PP.Z)
        return true;

    if (CachedEnemyMgr)
    {
        for (const AEnemy* E : CachedEnemyMgr->GetEnemies())
        {
            if (!E || !E->IsAlive()) continue;
            const FGridPos EP = E->GetPosition();
            const int32 EW = (E->Type == EEnemyType::Heavy) ? 2 : 1;
            const int32 EH = (E->Type == EEnemyType::Heavy) ? 2 : 1;
            if (Pos.X >= EP.X && Pos.X < EP.X + EW &&
                Pos.Y >= EP.Y - (EH - 1) && Pos.Y <= EP.Y &&
                Pos.Z == EP.Z)
                return true;
        }
    }
    return false;
}

// 滑鼠左鍵鬆開 / 輸入中斷：取消採掘進度（對應 Main.cs:1244-1247 else 分支）
void ASkillCreatorCharacter::OnMineReleased()
{
    bMineWasPressed = false;
    CancelMining();
}

// 短按左鍵：bIsConsumable 物品＝使用道具；其餘（含空手/工具）＝攻擊（docs/plan-item-crafting-system.md
// §衝突3 開放問題 Q3：攻擊判定屬於 plan-player-actions.md §E 攻擊框架範疇，這裡先留 stub，
// 不阻塞本計畫其餘部分，等該文件的攻擊框架決策確定後再接上真正的傷害判定）。
void ASkillCreatorCharacter::OnPrimaryTap()
{
    if (!InventoryComp || bIsCarrying) return;  // 拿取中不可攻擊
    const FItemStack& Active = InventoryComp->GetActiveItem();
    if (!Active.IsEmpty() && FItemRegistry::Get(Active.ItemId).bIsConsumable)
    {
        UseConsumable();
        return;
    }
    // 攻擊 stub：bIsWeapon 物品之後接 Strength*AtkMult 傷害公式，見 plan-player-actions.md
}

// 使用道具（左鍵長按/短按共用，docs/plan-item-crafting-system.md §衝突3 開放問題 Q4：
// 「道具」子分類規格沒有舉例，效果系統留待規格補充後設計，這裡先消耗 1 個並佔位）
void ASkillCreatorCharacter::UseConsumable()
{
    if (!InventoryComp) return;
    const int32 Idx = InventoryComp->ActiveHotbarIndex;
    if (!InventoryComp->Slots.IsValidIndex(Idx) || InventoryComp->Slots[Idx].IsEmpty()) return;

    const EItemId ItemId = InventoryComp->Slots[Idx].ItemId;
    InventoryComp->Consume(Idx, 1);

    // 物品元素：施加到使用者自身 AuraComp（藥水/食物的元素效果）
    if (AuraComp)
    {
        const ESkillElementType ItemElem = FItemRegistry::Get(ItemId).Element;
        if (ItemElem != ESkillElementType::None)
            AuraComp->ApplyImmediate(ItemElem, UElementalAuraComponent::DefaultAuraDuration, this);
    }
}

// 滑鼠右鍵鬆開：重置 rising-edge 狀態，讓下次按下能再次觸發放置（對應 Main.cs:1255 _rightWasPressed）
void ASkillCreatorCharacter::OnPlaceReleased()
{
    bRightMouseWasPressed = false;
}

// ── S-4 防禦/彈反 ───────────────────────────────────────────────────────────

void ASkillCreatorCharacter::PerformGuard()
{
    if (!IsAlive() || IsFlying()) return;
    MovementState = EPlayerMovementState::Guarding;
    ApplyMovementState();
    bInParryWindow = true;
    // 6 幀窗口（@ 60fps ≈ 100ms）；窗口結束後仍處於 Guarding 但不再彈反
    GetWorldTimerManager().SetTimer(ParryWindowTimer,
        FTimerDelegate::CreateLambda([this]() { bInParryWindow = false; }),
        ParryWindowSec, false);
}

void ASkillCreatorCharacter::EndGuard()
{
    if (!IsGuarding()) return;
    GetWorldTimerManager().ClearTimer(ParryWindowTimer);
    bInParryWindow = false;
    MovementState = EPlayerMovementState::Grounded;
    ApplyMovementState();
}

// ── S-8 後撤衝量（接口） ────────────────────────────────────────────────────

void ASkillCreatorCharacter::PerformBackDash()
{
    if (!IsAlive()) return;
    // 後撤衝量：4× 步行速度，覆寫 XY 並保留 Z 慣性
    const FVector Back     = -GetActorForwardVector();
    const float   DashSpd  = WorldScale::WalkSpeedCm * 4.f;
    LaunchCharacter(Back * DashSpd, true, false);
    // S-8 殘影 FX（stub — 實作後在 SpawnAfterimage 裡生成半透明網格）
    if (AfterimageComp)
        AfterimageComp->SpawnAfterimage(GetActorLocation(), Back, 0.3f);
}

// ── S-2 完整攻擊框架 ───────────────────────────────────────────────────────

void ASkillCreatorCharacter::StartChargingAttack()
{
    if (!IsAlive() || IsGuarding() || bIsCarrying) return;  // 拿取中不可基礎攻擊
    bChargingAttack  = true;
    AttackChargeTimer = 0.f;
}

void ASkillCreatorCharacter::ReleaseAttack()
{
    if (!bChargingAttack) return;
    const float Held = AttackChargeTimer;
    bChargingAttack  = false;
    AttackChargeTimer = 0.f;

    if (Held < LightAttackMaxCharge)
    {
        PerformLightAttack();
    }
    else
    {
        // 蓄力攻：傷害 = 20~80 × AtkMult，線性插值；半徑 1~2.5 遊戲單位
        if (!IsAlive() || !CachedEnemyMgr) return;
        const float ChargeRatio = FMath::Clamp((Held - LightAttackMaxCharge) / (MaxChargeTime - LightAttackMaxCharge), 0.f, 1.f);
        const FItemStack& AW2 = InventoryComp ? InventoryComp->GetActiveItem() : FItemStack{};
        const float AtkMult   = (!AW2.IsEmpty() && FItemRegistry::Get(AW2.ItemId).bIsWeapon)
                                    ? FItemRegistry::Get(AW2.ItemId).AtkMult : 1.f;
        const float PhysAtk   = FMath::Lerp(20.f, 80.f, ChargeRatio) * AtkMult;
        const float GameUnit  = WorldScale::TileSizeCm * WorldScale::GrainCurrent;
        const float Radius    = FMath::Lerp(GameUnit, GameUnit * 2.5f, ChargeRatio);
        const FVector Center  = GetActorLocation() + GetActorForwardVector() * GameUnit * 1.5f;

        auto* GI  = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
        auto* Sub = GI ? GI->GetSubsystem<UCombatStateSubsystem>() : nullptr;

        for (AEnemy* E : CachedEnemyMgr->GetEnemies())
        {
            if (!E || !E->IsAlive()) continue;
            if (FVector::Dist(Center, E->GetActorLocation()) <= Radius)
            {
                E->TakePhysicalDamage(PhysAtk, &Stats);
                if (Sub) Sub->OnPlayerDealtDamage(PhysAtk);
                OnAttackHit.Broadcast(EAttackType::Charged, E);
            }
        }
    }
}

void ASkillCreatorCharacter::PerformHeavyAttack()
{
    bChargingAttack   = false;
    AttackChargeTimer = 0.f;
    if (!IsAlive() || !CachedEnemyMgr) return;

    // 重攻：2.5× 基礎，範圍 1.5× 普通輕攻
    const FItemStack& AW3 = InventoryComp ? InventoryComp->GetActiveItem() : FItemStack{};
    const float AtkMult   = (!AW3.IsEmpty() && FItemRegistry::Get(AW3.ItemId).bIsWeapon)
                                ? FItemRegistry::Get(AW3.ItemId).AtkMult : 1.f;
    const float PhysAtk   = FMath::Max(50.f * AtkMult, Stats.Strength * 2.5f);
    const float GameUnit = WorldScale::TileSizeCm * WorldScale::GrainCurrent;
    const FVector Center = GetActorLocation() + GetActorForwardVector() * GameUnit * 1.5f;
    const float Radius   = GameUnit * 1.5f;

    auto* GI  = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    auto* Sub = GI ? GI->GetSubsystem<UCombatStateSubsystem>() : nullptr;

    for (AEnemy* E : CachedEnemyMgr->GetEnemies())
    {
        if (!E || !E->IsAlive()) continue;
        if (FVector::Dist(Center, E->GetActorLocation()) <= Radius)
        {
            E->TakePhysicalDamage(PhysAtk, &Stats);
            if (Sub) Sub->OnPlayerDealtDamage(PhysAtk);
            OnAttackHit.Broadcast(EAttackType::Heavy, E);
        }
    }
}

void ASkillCreatorCharacter::CancelAttack()
{
    bChargingAttack   = false;
    AttackChargeTimer = 0.f;
}

// ── S-1 擴展移動狀態 ──────────────────────────────────────────────────────

void ASkillCreatorCharacter::PerformCrouch()
{
    if (IsFlying() || IsRolling() || IsSliding() || IsCrouching()) return;
    PreActionState = MovementState;
    MovementState  = EPlayerMovementState::Crouching;
    ApplyMovementState();
}

void ASkillCreatorCharacter::EndCrouch()
{
    if (!IsCrouching()) return;
    MovementState = EPlayerMovementState::Grounded;
    ApplyMovementState();
}

void ASkillCreatorCharacter::PerformRoll()
{
    if (!IsAlive() || IsFlying() || IsRolling() || IsSliding()) return;
    PreActionState = MovementState;
    MovementState  = EPlayerMovementState::Rolling;
    ApplyMovementState();

    // 0.3 秒無敵幀（DamageShield "Cancel"）
    ActionBus.RegisterFilter(TEXT("DamageShield"), TEXT("Cancel"), false, 0.3f, 0.f);

    // 前衝衝量
    LaunchCharacter(GetActorForwardVector() * WorldScale::WalkSpeedCm * 2.f, false, false);

    // 0.5 秒後恢復
    GetWorldTimerManager().SetTimer(RollTimer, FTimerDelegate::CreateLambda([this]()
    {
        if (!IsRolling()) return;
        MovementState = PreActionState;
        ApplyMovementState();
    }), 0.5f, false);
}

void ASkillCreatorCharacter::PerformSlide()
{
    if (!IsAlive() || IsFlying() || IsRolling() || IsSliding()) return;
    PreActionState = MovementState;
    MovementState  = EPlayerMovementState::Sliding;
    ApplyMovementState();

    // 沿當前方向給予大衝量（維持疾跑慣性）
    LaunchCharacter(GetActorForwardVector() * WorldScale::WalkSpeedCm * 4.f, true, false);

    // 1 秒後自動結束（或主動按 X Released）
    GetWorldTimerManager().SetTimer(RollTimer, FTimerDelegate::CreateLambda([this]()
    {
        EndSlide();
    }), 1.0f, false);
}

void ASkillCreatorCharacter::EndSlide()
{
    if (!IsSliding()) return;
    GetWorldTimerManager().ClearTimer(RollTimer);
    MovementState = EPlayerMovementState::Grounded;
    ApplyMovementState();
}

void ASkillCreatorCharacter::StartFastFall()
{
    if (IsFlying()) { FlyDown(); return; }
    if (!GetCharacterMovement()->IsFalling()) return;
    LaunchCharacter(FVector(0.f, 0.f, -WorldScale::WalkSpeedCm * 3.f), false, true);
}

// ============================================================
// W-G：雜草 random tick（MC 機制）
// 每 GameTick，對 loaded chunk 取 3 次隨機樣本，10% 機率在 Grass+Air 上生成雜草 Entity
// 自限：ActiveWeedTiles 防重複；雜草 Actor 自毀後同步清除此 Set
// ============================================================
void ASkillCreatorCharacter::TickWeedGrowth(float DeltaTime)
{
    if (!CachedVoxelWorld) return;
    FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld();
    if (!TW) return;

    const auto& Chunks = TW->GetActiveChunks();
    if (Chunks.IsEmpty()) return;

    constexpr int32 S             = WorldScale::ChunkSize;
    constexpr int32 SamplesPerTick = 3;    // MC：每 section 每 tick 3 次

    // 用累加器實現「每 GameTick 而非每 DeltaTime」一次（對應 MC 每遊戲刻觸發）
    // GameClock 沒有引用，這裡用 DeltaTime 直接每幀跑（低頻 sampling，影響不大）

    // 從所有已加載的 chunk 中，等機率隨機取樣 SamplesPerTick 次
    const int32 ChunkCount = Chunks.Num();
    uint32 Seed = (uint32)(GFrameCounter ^ (uint64)this);
    auto RandU = [&]() -> uint32 { return Seed = Seed * 1664525u + 1013904223u; };

    for (int32 s = 0; s < SamplesPerTick; ++s)
    {
        // 取隨機 chunk
        int32 ChunkIdx = (int32)(RandU() % (uint32)ChunkCount);
        int32 Cur = 0;
        FIntVector CC = FIntVector::ZeroValue;
        for (const auto& Pair : Chunks)
        {
            if (Cur++ == ChunkIdx) { CC = Pair.Key; break; }
        }

        // 在該 chunk 內取隨機 XZ 座標，掃描地表（向下找 Grass）
        const int32 lx = (int32)(RandU() % (uint32)S);
        const int32 lz = (int32)(RandU() % (uint32)S);
        const int32 wx = CC.X * S + lx;
        const int32 wz = CC.Z * S + lz;

        // 找 chunk 內的 Grass tile（Y 由上往下掃，找第一個 Grass）
        int32 SurfaceY = -1;
        for (int32 wy = CC.Y * S; wy < (CC.Y + 1) * S; ++wy)
        {
            if (TW->GetTile(wx, wy, wz) == EMaterialType::Grass)
            {
                // 確認正上方是 Air
                if (TW->GetTile(wx, wy - 1, wz) == EMaterialType::Air)
                {
                    SurfaceY = wy;
                    break;
                }
            }
        }
        if (SurfaceY < 0) continue;

        // 10% 成功率
        if ((RandU() % 10) != 0) continue;

        // 防重複
        const FIntVector WeedTile(wx, SurfaceY, wz);
        if (ActiveWeedTiles.Contains(WeedTile)) continue;

        // 生成 AWeedEntity（世界位置 = tile XZ × TileSizeCm，Z = (WorldHeight - SurfaceY) × TileSizeCm）
        const float PosX = wx * WorldScale::TileSizeCm;
        const float PosY = wz * WorldScale::TileSizeCm;
        const float PosZ = (TW->Height - SurfaceY) * WorldScale::TileSizeCm;
        const FVector SpawnLoc(PosX, PosY, PosZ);

        AWeedEntity* Weed = GetWorld()->SpawnActor<AWeedEntity>(AWeedEntity::StaticClass(),
            FTransform(FRotator::ZeroRotator, SpawnLoc));
        if (Weed)
        {
            Weed->AnchorTile = WeedTile;
            ActiveWeedTiles.Add(WeedTile);

            // 雜草自毀時清除記錄（OnDestroyed delegate 自動呼叫）
            Weed->OnDestroyed.AddDynamic(this, &ASkillCreatorCharacter::OnWeedDestroyed);
        }
    }
}

void ASkillCreatorCharacter::OnWeedDestroyed(AActor* DestroyedActor)
{
    if (const AWeedEntity* Weed = Cast<AWeedEntity>(DestroyedActor))
        ActiveWeedTiles.Remove(Weed->AnchorTile);
}

// ── G-4 拿取狀態 ──────────────────────────────────────────────────────────

void ASkillCreatorCharacter::BeginCarry(AActor* Target)
{
    IPhysicalPickable* P = Cast<APhysicalItemActor>(Target);
    if (!P || !P->IsPickable()) return;
    bIsCarrying = true;
    CarriedActor = Target;
    P->OnCarried(this);
}

void ASkillCreatorCharacter::EndCarry(FVector ThrowVelocityCms)
{
    if (!CarriedActor.IsValid())
    {
        bIsCarrying = false;
        return;
    }
    IPhysicalPickable* P = Cast<APhysicalItemActor>(CarriedActor.Get());
    if (P) P->OnReleased(ThrowVelocityCms);
    bIsCarrying  = false;
    CarriedActor = nullptr;
}

AActor* ASkillCreatorCharacter::FindNearestPickable() const
{
    const float PickupRadiusCm = WorldScale::TileSizeCm * 3.f;
    TArray<AActor*> Overlaps;
    GetOverlappingActors(Overlaps);
    AActor* Best = nullptr;
    float BestDist = PickupRadiusCm;
    for (AActor* A : Overlaps)
    {
        IPhysicalPickable* P = Cast<APhysicalItemActor>(A);
        if (!P || !P->IsPickable()) continue;
        const float D = FVector::Dist(GetActorLocation(), A->GetActorLocation());
        if (D < BestDist) { BestDist = D; Best = A; }
    }
    return Best;
}
