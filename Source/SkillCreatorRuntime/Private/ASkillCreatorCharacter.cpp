#include "ASkillCreatorCharacter.h"
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
#include "GridPos.h"
#include "UDroppedItemManager.h"
#include "UEquipmentComponent.h"
#include "ItemRegistry.h"
#include "ItemData.h"
#include "EquipmentSlotType.h"
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

    // 遊戲內時間歸零（新局開始）
    if (auto* GI = GetWorld()->GetGameInstance())
    if (auto* Clock = GI->GetSubsystem<UGameClockSubsystem>())
        Clock->Reset();

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
        TW->OnTileDestroyed = [WeakWorld, WeakVoxelWorld](int32 x, int32 y, int32 z,
                                           EMaterialType OldMat, EDestroyReason Reason)
        {
            UWorld* W = WeakWorld.Get();
            if (!W) return;

            // K-5：任何方式摧毀的格子都要通知 Registry（不限完美移除路徑），
            // 對應 Godot Main.cs:404-407 全域 OnTileDestroyed → NotifyDestroyed
            if (AVoxelWorldActor* VW = WeakVoxelWorld.Get())
                VW->GetPlacedRegistry().NotifyDestroyed(FIntVector(x, y, z));

            auto* DropMgr = W->GetSubsystem<UDroppedItemManager>();
            if (!DropMgr) return;
            DropMgr->SpawnForReason(x, y, z, OldMat, Reason);
        };
    }
}

void ASkillCreatorCharacter::FillSaveData(FCharacterSaveData& OutData) const
{
    OutData.Level    = Level;
    OutData.Xp       = Xp;
    OutData.CurrentHp = CurrentHp;
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
    CurrentHp = Data.CurrentHp;
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
                bool bFree = false;
                if      (D.EquipSlot == EEquipmentSlotType::Weapon)    bFree = EquipmentComp->WeaponId    == EItemId::None;
                else if (D.EquipSlot == EEquipmentSlotType::Armor)     bFree = EquipmentComp->ArmorId     == EItemId::None;
                else if (D.EquipSlot == EEquipmentSlotType::Accessory) bFree = EquipmentComp->AccessoryId == EItemId::None;
                if (bFree && D.EquipSlot != EEquipmentSlotType::None)
                    EquipmentComp->TryEquip(InventoryComp, SlotIdx);
            }
        }
    }

    // 採掘高亮：每幀追蹤玩家視線，顯示即將被採掘的 tile
    if (CachedVoxelWorld)
    {
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld();
            FVector CamLoc; FRotator CamRot;
            PC->GetPlayerViewPoint(CamLoc, CamRot);
            FRaycastResult3D H = TW->Raycast(
                WorldScale::WorldToTileF(CamLoc, TW->Height),
                WorldScale::DirToVoxel(CamRot.Vector()),
                static_cast<float>(WorldScale::MiningRangeTiles));
            if (H.bHit)
                CachedVoxelWorld->ShowHighlight(FGridPos(H.HitCell.X, H.HitCell.Y, H.HitCell.Z));
            else
                CachedVoxelWorld->HideHighlight();
        }
    }

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

// B-3：物理傷害管線（防禦/減傷 → 暴擊判定 → 命中/閃避）
// 對應 Godot 設計文件 base value system.txt 第 24-29 行物理 2 步公式
void ASkillCreatorCharacter::TakePhysicalDamage(float PhysAtk, const FCharacterStats* Atk)
{
    // 命中/閃避判定
    if (Atk)
    {
        if (Atk->HitRate < 1.f && FMath::FRand() > Atk->HitRate) return;
        float ExcessHit  = FMath::Max(0.f, Atk->HitRate - 1.f);
        float EffDodge   = FMath::Max(0.f, Stats.DodgeRate - ExcessHit);
        if (FMath::FRand() < EffDodge) return;
    }

    // 物理 2 步防禦（設計文件 step1/step2）
    float Step1 = FMath::Max(0.f, PhysAtk - Stats.PhysicalDefense);
    float Final = FMath::Max(0.f, Step1 - Stats.PhysicalDamageReduction);

    // 暴擊判定（承受方的 AntiCrit/AntiCritDmg 抵銷攻擊方）
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

    TakeDirectDamage(Final);
}

// B-3：能量傷害管線（特定MP防禦 → 通用能量防禦 → 特定MP減傷 → 通用能量減傷 → 暴擊/閃避）
// 對應設計文件 base value system.txt 第 29 行能量 4 步公式
void ASkillCreatorCharacter::TakeEnergyDamage(float EnergyAtk, FName ManaTypeKey, const FCharacterStats* Atk)
{
    // 命中/閃避判定
    if (Atk)
    {
        if (Atk->HitRate < 1.f && FMath::FRand() > Atk->HitRate) return;
        float ExcessHit  = FMath::Max(0.f, Atk->HitRate - 1.f);
        float EffDodge   = FMath::Max(0.f, Stats.DodgeRate - ExcessHit);
        if (FMath::FRand() < EffDodge) return;
    }

    // 能量 4 步防禦
    float Step1 = FMath::Max(0.f, EnergyAtk  - Stats.GetMpDefense(ManaTypeKey));   // 特定MP防禦
    float Step2 = FMath::Max(0.f, Step1       - Stats.EnergyDefense);               // 通用能量防禦
    float Step3 = FMath::Max(0.f, Step2       - Stats.GetMpDamageReduction(ManaTypeKey)); // 特定MP減傷
    float Final = FMath::Max(0.f, Step3       - Stats.EnergyDamageReduction);       // 通用能量減傷

    // 暴擊判定
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

    TakeDirectDamage(Final);
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
    EMaterialType Tile = TW->GetTile(Pos.X, Pos.Y, Pos.Z);

    float Dps = 0.f;
    if (Tile == EMaterialType::Fire)
        Dps = AEnemyManager::FireDps;
    else if (Tile == EMaterialType::Lava)
        Dps = AEnemyManager::LavaDps;

    if (Dps > 0.f)
        TakeDirectDamage(Dps * DeltaTime);
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
    IMC->MapKey(IA_Mine,      EKeys::LeftMouseButton);
    IMC->MapKey(IA_Place,     EKeys::RightMouseButton);
    IMC->MapKey(IA_SpellU,    EKeys::U);
    IMC->MapKey(IA_SpellI,    EKeys::I);
    IMC->MapKey(IA_SpellO,    EKeys::O);
    IMC->MapKey(IA_SpellP,    EKeys::P);
    IMC->MapKey(IA_Camera,    EKeys::Tab);
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
    EIC->BindAction(IA_Jump,  ETriggerEvent::Started,    this, &ACharacter::Jump);
    EIC->BindAction(IA_Jump,  ETriggerEvent::Completed,  this, &ACharacter::StopJumping);
    EIC->BindAction(IA_Mine,  ETriggerEvent::Triggered,  this, &ASkillCreatorCharacter::OnMine);
    EIC->BindAction(IA_Mine,  ETriggerEvent::Completed,  this, &ASkillCreatorCharacter::OnMineReleased);
    EIC->BindAction(IA_Mine,  ETriggerEvent::Canceled,   this, &ASkillCreatorCharacter::OnMineReleased);
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
    const FRotator YawOnly(0.f, GetControlRotation().Yaw, 0.f);
    if (Axis.Y != 0.f)
        AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X), Axis.Y);
    if (Axis.X != 0.f)
        AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y), Axis.X);
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
    if (!SpellCasterComp) return;
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

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
    if (!CachedVoxelWorld) { CancelMining(); return; }
    FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld();
    if (!TW) { CancelMining(); return; }
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) { CancelMining(); return; }

    // K-22：游標在熱鍵欄格上時阻斷採掘（Godot Main.cs:1198-1199 _mouseOverHotbar）
    if (const ASkillCreatorHUD* HUD = PC->GetHUD<ASkillCreatorHUD>())
        if (HUD->bMouseOverHotbar) { CancelMining(); return; }

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
    const float SpeedMult = InventoryComp ? InventoryComp->GetActiveMiningSpeedMult() : 1.f;
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

void ASkillCreatorCharacter::OnPlace()
{
    if (!CachedVoxelWorld || !InventoryComp) return;
    FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld();
    if (!TW) return;
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    // 放置節流：_holdToPlace=true 時 0.12 秒節流連放，false 時 rising-edge
    // （對應 Main.cs:1250-1255）。HUD->bHoldToPlace 預設 false（rising-edge），
    // Enhanced Input 的 Triggered 事件每幀觸發，故節流邏輯需要角色自己持有計時器。
    ASkillCreatorHUD* HUD = Cast<ASkillCreatorHUD>(PC->GetHUD());
    // K-22：游標在熱鍵欄格上時阻斷放置（Godot Main.cs:1256 _mouseOverHotbar）
    if (HUD && HUD->bMouseOverHotbar) return;
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
    if (!Data.bIsPlaceable || Data.PlaceAs == EMaterialType::Air) return;

    // 放置中心 = 命中面法線方向的相鄰格（對應 Main.cs:1262 MouseGridPos + MouseFaceNormal）
    const FIntVector PlaceCenter = Hit.HitCell + Hit.FaceNormal;

    EPlacementShape Shape = EPlacementShape::Cube;
    int32 Radius = WorldScale::DefaultShapeRadius;
    if (HUD)
    {
        Shape  = HUD->ActiveShape;
        Radius = HUD->PlaceRadius;
    }

    const FGridPos PlayerPos = GetPosition();
    FPlacedObjectRegistry& Registry = CachedVoxelWorld->GetPlacedRegistry();

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
    CancelMining();
}

// 滑鼠右鍵鬆開：重置 rising-edge 狀態，讓下次按下能再次觸發放置（對應 Main.cs:1255 _rightWasPressed）
void ASkillCreatorCharacter::OnPlaceReleased()
{
    bRightMouseWasPressed = false;
}
