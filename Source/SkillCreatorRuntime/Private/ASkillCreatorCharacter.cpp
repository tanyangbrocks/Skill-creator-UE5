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
#include "MaterialRegistry.h"
#include "ItemDrop.h"
#include "WorldScale.h"
#include "GameFramework/SpringArmComponent.h"
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

    // Camera rig: SpringArm follows controller rotation (Minecraft style)
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 600.f;
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 64.f));

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

    CachedVoxelWorld = AVoxelWorldActor::FindInWorld(GetWorld());

    // 快取 EnemyManager（關卡可能手動放置或由 GameMode 自動生成）
    for (TActorIterator<AEnemyManager> It(GetWorld()); It; ++It)
    { CachedEnemyMgr = *It; break; }

    // 綁定採掘掉落回呼（從 SkillCreatorRuntime 側接入，避免循環依賴）
    if (CachedVoxelWorld)
    if (FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld())
    {
        TWeakObjectPtr<UWorld> WeakWorld(GetWorld());
        TW->OnTileDestroyed = [WeakWorld](int32 x, int32 y, int32 z,
                                           EMaterialType OldMat, EDestroyReason Reason)
        {
            UWorld* W = WeakWorld.Get();
            if (!W) return;
            auto* DropMgr = W->GetSubsystem<UDroppedItemManager>();
            if (!DropMgr) return;
            FRandomStream Rng;
            Rng.Initialize(FMath::Rand());
            for (const FItemDrop& D : FMaterialRegistry::GetDefaultDrops(OldMat))
                if (D.ItemId != EItemId::None && Rng.FRand() <= D.Chance)
                    DropMgr->SpawnDrop(D.ItemId, Rng.RandRange(D.MinCount, D.MaxCount),
                                       FGridPos(x, y, z));
        };
    }

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
    for (FManaSlot& Slot : ActiveManaSlots) Slot.Tick(DeltaTime);
    if (ActiveManaSlots.Num() > 0) CurrentMp = ActiveManaSlots[0].Current;

    // HP 自然回復（若有設定）
    if (Stats.HpRegenRate > 0.f && CurrentHp > 0.f)
        CurrentHp = FMath::Min(Stats.MaxHpBase, CurrentHp + Stats.HpRegenRate * DeltaTime);

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

    // 自動拾取玩家周圍掉落物
    if (auto* DropMgr = GetWorld()->GetSubsystem<UDroppedItemManager>())
    {
        TArray<FItemStack> Picked = DropMgr->TryPickupAll(this);
        for (const FItemStack& S : Picked)
            if (InventoryComp && S.ItemId != EItemId::None)
                InventoryComp->TryAdd(S.ItemId, S.Count);
    }
}

float ASkillCreatorCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    float Modified = DamageAmount * (1.f + AuraComp->DamageTakenBonus);
    float Final    = FMath::Max(0.f, Modified - Stats.BaseDefense * (1.f - AuraComp->DefensePenalty));
    TakeDirectDamage(Final);
    return Final;
}

void ASkillCreatorCharacter::TakeDirectDamage(float Amount)
{
    if (CurrentHp <= 0.f) return;

    // DamageShield 攔截：可能將傷害歸零或縮減
    float Final = ActionBus.DispatchPlayerDamage(Amount);

    CurrentHp = FMath::Max(0.f, CurrentHp - Final);
    OnHpChanged.Broadcast(CurrentHp);

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

FGridPos ASkillCreatorCharacter::GetPosition() const
{
    FVector Loc = GetActorLocation();
    // UE5→voxel: X→X, Y→Z(depth), Z→WorldH-Y(vertical inverted)
    const float InvS = 1.f / WorldScale::TileSizeCm;
    const int32 VoxX = FMath::RoundToInt(Loc.X * InvS);
    const int32 VoxZ = FMath::RoundToInt(Loc.Y * InvS);
    int32 VoxY = 128; // 若 VoxelWorld 不可用則用近似中心高度
    if (CachedVoxelWorld)
        if (FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld())
            VoxY = FMath::RoundToInt(static_cast<float>(TW->Height) - Loc.Z * InvS);
    return FGridPos(VoxX, VoxY, VoxZ);
}

void ASkillCreatorCharacter::ApplyEnvironmentalDamage(float DeltaTime)
{
    if (CurrentHp <= 0.f || !CachedVoxelWorld) return;
    FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld();
    if (!TW) return;

    // 直接在此做 UE5→voxel 轉換，不依賴 GetPosition() 的近似值
    const FVector Loc = GetActorLocation();
    const float InvS = 1.f / WorldScale::TileSizeCm;
    const int32 VoxX = FMath::RoundToInt(Loc.X * InvS);
    const int32 VoxY = FMath::RoundToInt(static_cast<float>(TW->Height) - Loc.Z * InvS);
    const int32 VoxZ = FMath::RoundToInt(Loc.Y * InvS);
    EMaterialType Tile = TW->GetTile(VoxX, VoxY, VoxZ);

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
    UInputAction* IA_SpellU    = MakeBool(TEXT("SpellU"));
    UInputAction* IA_SpellI    = MakeBool(TEXT("SpellI"));
    UInputAction* IA_SpellO    = MakeBool(TEXT("SpellO"));
    UInputAction* IA_SpellP    = MakeBool(TEXT("SpellP"));
    UInputAction* IA_Camera    = MakeBool(TEXT("ToggleCamera"));
    UInputAction* IA_Inv       = MakeBool(TEXT("OpenInventory"));
    UInputAction* IA_Equip     = MakeBool(TEXT("OpenEquipment"));
    UInputAction* IA_CharPanel = MakeBool(TEXT("OpenCharPanel"));
    UInputAction* IA_DbgTrace  = MakeBool(TEXT("DebugTrace"));
    UInputAction* IA_SnapTake  = MakeBool(TEXT("SnapshotTake"));
    UInputAction* IA_SnapApply = MakeBool(TEXT("SnapshotApply"));

    UInputAction* IA_Move = NewObject<UInputAction>(this, TEXT("IA_Move"));
    IA_Move->ValueType = EInputActionValueType::Axis2D;

    UInputAction* IA_Look = NewObject<UInputAction>(this, TEXT("IA_Look"));
    IA_Look->ValueType = EInputActionValueType::Axis2D;

    // ── 建立 InputMappingContext ──────────────────────────────────────────
    UInputMappingContext* IMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Default"));

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
        FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Look, EKeys::MouseY);
        UInputModifierSwizzleAxis* Sw = NewObject<UInputModifierSwizzleAxis>(IMC);
        Sw->Order = EInputAxisSwizzle::YXZ;
        M.Modifiers.Add(Sw);
    }

    // 其他按鍵
    IMC->MapKey(IA_Jump,      EKeys::SpaceBar);
    IMC->MapKey(IA_Mine,      EKeys::LeftMouseButton);
    IMC->MapKey(IA_SpellU,    EKeys::U);
    IMC->MapKey(IA_SpellI,    EKeys::I);
    IMC->MapKey(IA_SpellO,    EKeys::O);
    IMC->MapKey(IA_SpellP,    EKeys::P);
    IMC->MapKey(IA_Camera,    EKeys::Tab);
    IMC->MapKey(IA_Inv,       EKeys::Z);
    IMC->MapKey(IA_Equip,     EKeys::X);
    IMC->MapKey(IA_CharPanel, EKeys::C);
    IMC->MapKey(IA_DbgTrace,  EKeys::F3);
    IMC->MapKey(IA_SnapTake,  EKeys::F5);
    IMC->MapKey(IA_SnapApply, EKeys::F6);

    // ── 注冊 MappingContext ───────────────────────────────────────────────
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
        if (auto* Sub = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            Sub->AddMappingContext(IMC, 0);

    // ── 綁定 Actions ─────────────────────────────────────────────────────
    EIC->BindAction(IA_Move, ETriggerEvent::Triggered,  this, &ASkillCreatorCharacter::Move);
    EIC->BindAction(IA_Look, ETriggerEvent::Triggered,  this, &ASkillCreatorCharacter::Look);
    EIC->BindAction(IA_Jump, ETriggerEvent::Started,    this, &ACharacter::Jump);
    EIC->BindAction(IA_Jump, ETriggerEvent::Completed,  this, &ACharacter::StopJumping);
    EIC->BindAction(IA_Mine, ETriggerEvent::Started,    this, &ASkillCreatorCharacter::OnMine);

    EIC->BindAction(IA_SpellU, ETriggerEvent::Started, this, &ASkillCreatorCharacter::HandleSpellInput);
    EIC->BindAction(IA_SpellI, ETriggerEvent::Started, this, &ASkillCreatorCharacter::HandleSpellInput);
    EIC->BindAction(IA_SpellO, ETriggerEvent::Started, this, &ASkillCreatorCharacter::HandleSpellInput);
    EIC->BindAction(IA_SpellP, ETriggerEvent::Started, this, &ASkillCreatorCharacter::HandleSpellInput);

    EIC->BindAction(IA_Camera,    ETriggerEvent::Started, this, &ASkillCreatorCharacter::OnToggleCameraMode);
    EIC->BindAction(IA_Inv,       ETriggerEvent::Started, this, &ASkillCreatorCharacter::OnOpenInventory);
    EIC->BindAction(IA_Equip,     ETriggerEvent::Started, this, &ASkillCreatorCharacter::OnOpenEquipment);
    EIC->BindAction(IA_CharPanel, ETriggerEvent::Started, this, &ASkillCreatorCharacter::OnOpenCharacterPanel);
    EIC->BindAction(IA_DbgTrace,  ETriggerEvent::Started, this, &ASkillCreatorCharacter::OnDebugTrace);
    EIC->BindAction(IA_SnapTake,  ETriggerEvent::Started, this, &ASkillCreatorCharacter::OnDebugSnapshotTake);
    EIC->BindAction(IA_SnapApply, ETriggerEvent::Started, this, &ASkillCreatorCharacter::OnDebugSnapshotApply);
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
            break;
        case ECameraMode::SideScroll2D:
            P.ArmLength = 900.f; P.PitchDeg = -10.f; P.bUsePawnControlRotation = false;
            P.FixedYawDeg = 90.f;
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

void ASkillCreatorCharacter::OnOpenInventory()
{
    if (!InventoryComp || !GEngine) return;
    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, TEXT("=== 物品欄 [Z] ==="));
    bool bAny = false;
    for (int32 i = 0; i < InventoryComp->Slots.Num(); ++i)
    {
        const FItemStack& S = InventoryComp->Slots[i];
        if (S.IsEmpty()) continue;
        UEnum* E = StaticEnum<EItemId>();
        FString Name = E ? E->GetNameStringByValue((int64)S.ItemId) : FString::FromInt((int32)S.ItemId);
        GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::White,
            FString::Printf(TEXT("  [%d] %s x%d"), i, *Name, S.Count));
        bAny = true;
    }
    if (!bAny)
        GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::White, TEXT("  （空）"));
}

void ASkillCreatorCharacter::OnOpenEquipment()
{
    if (!EquipmentComp || !GEngine) return;
    UEnum* E = StaticEnum<EItemId>();
    auto Name = [&](EItemId Id) -> FString {
        return E ? E->GetNameStringByValue((int64)Id) : FString::FromInt((int32)Id);
    };
    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, TEXT("=== 裝備欄 [X] ==="));
    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::White,
        FString::Printf(TEXT("  武器:  %s  ATK×%.2f"), *Name(EquipmentComp->WeaponId),   EquipmentComp->GetTotalAtkMult()));
    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::White,
        FString::Printf(TEXT("  防具:  %s  DEF+%.0f"), *Name(EquipmentComp->ArmorId),    EquipmentComp->GetTotalDefFlat()));
    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::White,
        FString::Printf(TEXT("  飾品:  %s  MP+%.0f"), *Name(EquipmentComp->AccessoryId), EquipmentComp->GetTotalMpBonus()));
}

void ASkillCreatorCharacter::OnOpenCharacterPanel()
{
    if (!GEngine) return;
    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, TEXT("=== 角色狀態 [C] ==="));
    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::White,
        FString::Printf(TEXT("  等級: %d  境界: %s"), Level, *GetTierName(Level)));
    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::White,
        FString::Printf(TEXT("  XP: %.0f / %d"), Xp, XpRequired(Level)));
    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::White,
        FString::Printf(TEXT("  HP: %.0f / %.0f"), CurrentHp, Stats.MaxHpBase));
    GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::White,
        FString::Printf(TEXT("  MP: %.0f / %.0f"), CurrentMp, Stats.MaxMpBase));
    if (StateComp)
        GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::White,
            FString::Printf(TEXT("  體力: %.0f  心情: %.0f"),
                StateComp->Stamina, StateComp->Mood));
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

// ── Mining ──────────────────────────────────────────────────────────────

void ASkillCreatorCharacter::OnMine()
{
    if (!CachedVoxelWorld) return;
    FTileWorld3D* TW = CachedVoxelWorld->GetTileWorld();
    if (!TW) return;
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);
    const FVector CamDir = CamRot.Vector();

    // UE5→voxel 座標轉換：X→X, Y→Z(depth), Z→WorldH-Y(vertical inverted)
    const float InvS = 1.f / WorldScale::TileSizeCm;
    FVector TileStart;
    TileStart.X = CamLoc.X * InvS;
    TileStart.Y = static_cast<float>(TW->Height) - CamLoc.Z * InvS; // UE5 Z → voxel Y
    TileStart.Z = CamLoc.Y * InvS;                                    // UE5 Y → voxel Z

    // 方向向量同樣需要轉換
    FVector VoxDir;
    VoxDir.X = CamDir.X;
    VoxDir.Y = -CamDir.Z; // UE5 +Z(上) = voxel -Y(天頂方向)
    VoxDir.Z = CamDir.Y;  // UE5 +Y(右) = voxel +Z(深度)

    FRaycastResult3D Hit = TW->Raycast(TileStart, VoxDir, 10.f);
    if (!Hit.bHit) return;

    EMaterialType OldMat = TW->GetTile(Hit.HitCell.X, Hit.HitCell.Y, Hit.HitCell.Z);

    // DestroyTile 內部已觸發 OnTileDestroyed（掉落回呼），此處不重複派送
    TW->DestroyTile(Hit.HitCell.X, Hit.HitCell.Y, Hit.HitCell.Z, EDestroyReason::Mining);

    (void)OldMat; // 掉落已由 OnTileDestroyed lambda 統一處理
}
