#include "ASkillCreatorHUD.h"
#include "ASkillCreatorCharacter.h"
#include "AEnemy.h"
#include "USpellCaster.h"
#include "UCharacterStateComponent.h"
#include "UInventoryComponent.h"
#include "UEquipmentComponent.h"
#include "UPlayerPanelWidget.h"
#include "UPlayerSettingsWidget.h"
#include "UShapeMenuWidget.h"
#include "USpellGroupWidget.h"
#include "UInventoryWidget.h"
#include "UEquipmentWidget.h"
#include "UDebugPaintWidget.h"
#include "UChestWidget.h"
#include "UCraftingPanelWidget.h"
#include "UCraftingHintCardWidget.h"
#include "UCraftingStationSubsystem.h"
#include "APlacedFixtureActor.h"
#include "ItemRegistry.h"
#include "APhysicalItemActor.h"
#include "IPhysicalPickable.h"
#include "USpecialStatusComponent.h"
#include "ADebrisActor.h"
#include "UPhysicalThrowWidget.h"
#include "WorldScale.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "MaterialType.h"
#include "Engine/Canvas.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

// ── 建構 ─────────────────────────────────────────────────────────────────

ASkillCreatorHUD::ASkillCreatorHUD()
{
    HUDWidgetClass = UPlayerHUDWidget::StaticClass();
}

// ── 泛型面板建立 helper ───────────────────────────────────────────────────

template<typename T>
TObjectPtr<T> ASkillCreatorHUD::CreatePanel()
{
    APlayerController* PC = GetOwningPlayerController();
    if (!PC) return nullptr;
    T* W = CreateWidget<T>(PC, T::StaticClass());
    if (W)
    {
        W->AddToViewport();
        W->SetVisibility(ESlateVisibility::Collapsed);
    }
    return W;
}

// ── 視覺狀態 toggle ───────────────────────────────────────────────────────

static void TogglePanel(UWidget* W)
{
    if (!W) return;
    bool bVisible = W->GetVisibility() == ESlateVisibility::Visible
                 || W->GetVisibility() == ESlateVisibility::SelfHitTestInvisible
                 || W->GetVisibility() == ESlateVisibility::HitTestInvisible;
    W->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void ASkillCreatorHUD::TogglePlayerPanel()
{
    if (!PlayerPanel) return;
    const bool bWasVisible = PlayerPanel->GetVisibility() == ESlateVisibility::Visible;
    if (!bWasVisible)
    {
        PlayerPanel->SetVisibility(ESlateVisibility::Visible);
        if (APlayerController* PC = GetOwningPlayerController())
        {
            PC->SetShowMouseCursor(true);
            // UIOnly 讓所有滑鼠事件直接送到 Slate；GameAndUI 沒有 SetWidgetToFocus
            // 時 viewport 仍捕捉左鍵，Button.OnClicked 收不到（tab 切換失效的根因）
            FInputModeUIOnly UiMode;
            UiMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            UiMode.SetWidgetToFocus(PlayerPanel->TakeWidget());
            PC->SetInputMode(UiMode);
        }
    }
    else
    {
        PlayerPanel->SetVisibility(ESlateVisibility::Collapsed);
        if (APlayerController* PC = GetOwningPlayerController())
        {
            PC->SetShowMouseCursor(false);
            PC->SetInputMode(FInputModeGameOnly());
        }
    }
}

void ASkillCreatorHUD::ToggleInventoryAndEquipment()
{
    // 以 InventoryPanel 可見性為準決定開或關
    const bool bCurrentlyVisible = InventoryPanel &&
        InventoryPanel->GetVisibility() == ESlateVisibility::Visible;

    if (!bCurrentlyVisible)
    {
        ASkillCreatorCharacter* Char = GetOwningPlayerController()
            ? Cast<ASkillCreatorCharacter>(GetOwningPlayerController()->GetPawn()) : nullptr;
        if (Char)
        {
            if (InventoryPanel && Char->InventoryComp)
                InventoryPanel->Refresh(Char->InventoryComp);
            if (EquipmentPanel && Char->EquipmentComp)
                EquipmentPanel->Refresh(Char->EquipmentComp);
        }
        if (InventoryPanel) InventoryPanel->SetVisibility(ESlateVisibility::Visible);
        if (EquipmentPanel) EquipmentPanel->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        if (InventoryPanel) InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);
        if (EquipmentPanel) EquipmentPanel->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void ASkillCreatorHUD::ToggleShapeMenu()       { TogglePanel(ShapeMenuPanel);      }
void ASkillCreatorHUD::ToggleSpellGroup()
{
    if (!SpellGroupPanel) return;
    APlayerController* PC = GetOwningPlayerController();
    const bool bCurrentlyVisible = SpellGroupPanel->GetVisibility() == ESlateVisibility::Visible;
    if (bCurrentlyVisible)
    {
        SpellGroupPanel->SetVisibility(ESlateVisibility::Collapsed);
        if (PC) PC->SetShowMouseCursor(false);
    }
    else
    {
        SpellGroupPanel->SetVisibility(ESlateVisibility::Visible);
        if (PC)
        {
            PC->SetShowMouseCursor(true);
            PC->SetInputMode(FInputModeGameAndUI());
        }
    }
}
void ASkillCreatorHUD::ToggleInventory()       { TogglePanel(InventoryPanel);      }
void ASkillCreatorHUD::ToggleEquipment()       { TogglePanel(EquipmentPanel);      }
void ASkillCreatorHUD::ToggleDebugPaint()      { TogglePanel(DebugPaintPanel);     }

void ASkillCreatorHUD::ToggleCraftingPanel()
{
    if (!CraftingPanel) return;
    APlayerController* PC = GetOwningPlayerController();
    const bool bCurrentlyVisible = CraftingPanel->GetVisibility() == ESlateVisibility::Visible;
    if (bCurrentlyVisible)
    {
        CraftingPanel->SetVisibility(ESlateVisibility::Collapsed);
        if (PC) PC->SetShowMouseCursor(false);
    }
    else
    {
        CraftingPanel->SetVisibility(ESlateVisibility::Visible);
        if (PC)
        {
            PC->SetShowMouseCursor(true);
            PC->SetInputMode(FInputModeGameAndUI());
        }
    }
}

void ASkillCreatorHUD::OpenChest(AChestActor* Chest)
{
    if (!Chest || !ChestPanel) return;

    // 再按一次右鍵（同一個寶箱）＝關閉，符合 AChestActor::OpenChestUI 的 toggle 語意
    const bool bAlreadyOpenForThisChest =
        ChestPanel->GetVisibility() == ESlateVisibility::Visible && CurrentChestActor == Chest;
    if (bAlreadyOpenForThisChest)
    {
        CloseChest();
        return;
    }

    ASkillCreatorCharacter* Char = GetOwningPlayerController()
        ? Cast<ASkillCreatorCharacter>(GetOwningPlayerController()->GetPawn()) : nullptr;
    if (!Char || !Char->InventoryComp) return;

    CurrentChestActor = Chest;
    ChestPanel->Setup(Char->InventoryComp, Chest->InventoryComp);
    ChestPanel->SetVisibility(ESlateVisibility::Visible);

    // 規格「玩家打開寶箱，玩家自己的物品欄也會自動打開」
    if (InventoryPanel)
    {
        InventoryPanel->Refresh(Char->InventoryComp);
        InventoryPanel->SetVisibility(ESlateVisibility::Visible);
    }
}

void ASkillCreatorHUD::CloseChest()
{
    CurrentChestActor = nullptr;
    if (ChestPanel) ChestPanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ── BeginPlay：建立所有 widget + 綁定 delegate ───────────────────────────

void ASkillCreatorHUD::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PC = GetOwningPlayerController();
    if (!PC) return;

    // 常駐 HUD
    // 2026-06-23 診斷：使用者多次回報準心/物品熱鍵欄/生存條從未出現，懷疑 HUDWidgetClass
    // 在 Standalone 封裝版實際執行時並非預期的純 C++ UPlayerHUDWidget；先加 log 確認真相，
    // 而不是再猜一次（前幾輪 GameMode/PlayerController 鏈路檢查都顯示邏輯應該正確）。
    UE_LOG(LogTemp, Warning, TEXT("ASkillCreatorHUD::BeginPlay HUDWidgetClass=%s"),
        HUDWidgetClass ? *HUDWidgetClass->GetName() : TEXT("nullptr"));
    if (HUDWidgetClass)
    {
        HUDWidget = CreateWidget<UPlayerHUDWidget>(PC, HUDWidgetClass);
        UE_LOG(LogTemp, Warning, TEXT("ASkillCreatorHUD::BeginPlay HUDWidget created=%s class=%s"),
            HUDWidget ? TEXT("true") : TEXT("false"),
            HUDWidget ? *HUDWidget->GetClass()->GetName() : TEXT("n/a"));
        if (HUDWidget) HUDWidget->AddToViewport();
    }

    // ── 面板 widget ────────────────────────────────────────────────────

    // PlayerPanel（G 鍵：4 Tab + 引導/圖鑑/設定）
    PlayerPanel = CreatePanel<UPlayerPanelWidget>();
    if (PlayerPanel)
    {
        // 填滿視窗（與 UBlockEditorWidget 相同做法）
        PlayerPanel->SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
        PlayerPanel->SetAlignmentInViewport(FVector2D::ZeroVector);
        // 設定子面板 delegate（對應舊 SettingsPanel 的綁定）
        if (UPlayerSettingsWidget* SW = PlayerPanel->GetSettingsWidget())
        {
            SW->OnHoldToPlaceChanged.BindLambda(
                [this](bool b){ bHoldToPlace = b; });
            SW->OnPerfectRemoveChanged.BindLambda(
                [this](bool b){ bPerfectRemove = b; });
            SW->SyncState(bHoldToPlace, bPerfectRemove);
        }
    }

    // ShapeMenu
    ShapeMenuPanel = CreatePanel<UShapeMenuWidget>();
    if (ShapeMenuPanel)
    {
        ShapeMenuPanel->OnShapeSelected.BindLambda(
            [this](EPlacementShape S)
            {
                ActiveShape = S;
                if (HUDWidget) HUDWidget->SetActiveShape(S, PlaceRadius);
            });
        ShapeMenuPanel->OnCloseRequested.BindLambda(
            [this](){ if (ShapeMenuPanel) ShapeMenuPanel->SetVisibility(ESlateVisibility::Collapsed); });
    }

    // DebugPaint（K-12，F1 開關，無關閉按鈕——再按 F1 收起，對應 Godot 行為）
    DebugPaintPanel = CreatePanel<UDebugPaintWidget>();
    if (DebugPaintPanel)
    {
        DebugPaintPanel->ActiveMaterial = ActivePaintMaterial;
        DebugPaintPanel->BrushRadius    = PaintBrushRadius;
        DebugPaintPanel->OnMaterialSelected.BindLambda(
            [this](EMaterialType M){ ActivePaintMaterial = M; });
        DebugPaintPanel->OnBrushRadiusSelected.BindLambda(
            [this](int32 R){ PaintBrushRadius = R; });
    }

    // SpellGroup
    SpellGroupPanel = CreatePanel<USpellGroupWidget>();
    if (SpellGroupPanel)
    {
        SpellGroupPanel->OnGroupSelected.BindLambda(
            [this](int32 G)
            {
                APlayerController* PC = GetOwningPlayerController();
                ASkillCreatorCharacter* Char = PC ? Cast<ASkillCreatorCharacter>(PC->GetPawn()) : nullptr;
                if (Char && Char->SpellCasterComp)
                    Char->SpellCasterComp->SpellGroups.SetActiveGroup(G);
                if (SpellGroupPanel)
                    SpellGroupPanel->SetVisibility(ESlateVisibility::Collapsed);
                if (PC) PC->SetShowMouseCursor(false);
            });
        SpellGroupPanel->OnCloseRequested.BindLambda(
            [this]()
            {
                if (SpellGroupPanel) SpellGroupPanel->SetVisibility(ESlateVisibility::Collapsed);
                if (APlayerController* PC = GetOwningPlayerController()) PC->SetShowMouseCursor(false);
            });
    }

    // Inventory
    InventoryPanel = CreatePanel<UInventoryWidget>();
    if (InventoryPanel)
    {
        InventoryPanel->OnSlotSwapRequested.BindLambda(
            [this](int32 A, int32 B)
            {
                ASkillCreatorCharacter* Char = GetOwningPlayerController()
                    ? Cast<ASkillCreatorCharacter>(GetOwningPlayerController()->GetPawn()) : nullptr;
                if (Char && Char->InventoryComp)
                {
                    Char->InventoryComp->SwapSlots(A, B);
                    InventoryPanel->Refresh(Char->InventoryComp);
                }
            });
        InventoryPanel->OnSlotEquipRequested.BindLambda(
            [this](int32 Idx)
            {
                ASkillCreatorCharacter* Char = GetOwningPlayerController()
                    ? Cast<ASkillCreatorCharacter>(GetOwningPlayerController()->GetPawn()) : nullptr;
                if (Char && Char->InventoryComp && Char->EquipmentComp)
                {
                    Char->EquipmentComp->TryEquip(Char->InventoryComp, Idx);
                    InventoryPanel->Refresh(Char->InventoryComp);
                    if (EquipmentPanel) EquipmentPanel->Refresh(Char->EquipmentComp);
                }
            });
    }

    // Equipment
    EquipmentPanel = CreatePanel<UEquipmentWidget>();
    if (EquipmentPanel)
    {
        EquipmentPanel->OnUnequipRequested.BindLambda(
            [this](FName SlotId)
            {
                ASkillCreatorCharacter* Char = GetOwningPlayerController()
                    ? Cast<ASkillCreatorCharacter>(GetOwningPlayerController()->GetPawn()) : nullptr;
                if (Char && Char->EquipmentComp && Char->InventoryComp)
                {
                    Char->EquipmentComp->TryUnequip(Char->InventoryComp, SlotId);
                    EquipmentPanel->Refresh(Char->EquipmentComp);
                    if (InventoryPanel) InventoryPanel->Refresh(Char->InventoryComp);
                }
            });
    }

    // 寶箱雙欄面板（docs/plan-item-crafting-system.md §六；非常駐 Toggle，由 OpenChest() 帶資料開啟）
    ChestPanel = CreatePanel<UChestWidget>();

    // 加工選單提示圖卡（常駐左側，HP 圓形下方；CreatePanel 預設 Collapsed，必須手動改回 Visible）
    CraftingHintCard = CreatePanel<UCraftingHintCardWidget>();
    if (CraftingHintCard) CraftingHintCard->SetVisibility(ESlateVisibility::Visible);

    // 加工選單完整面板（Shift 展開/收起，預設 Collapsed）
    CraftingPanel = CreatePanel<UCraftingPanelWidget>();

    // 投擲力量條（F 長按時才顯示，預設 Collapsed）
    ThrowWidget = CreatePanel<UPhysicalThrowWidget>();
}

// ── DrawHUD：每幀餵資料 ───────────────────────────────────────────────────

void ASkillCreatorHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!HUDWidget) return;

    APlayerController* PC = GetOwningPlayerController();
    if (!PC) return;

    ASkillCreatorCharacter* Char =
        PC->GetPawn() ? Cast<ASkillCreatorCharacter>(PC->GetPawn()) : nullptr;
    if (!Char) return;

    // ── HP / MP / Stamina / Mood ──────────────────────────────────────
    float StaminaPct = Char->StateComp
        ? Char->StateComp->Stamina / UCharacterStateComponent::MaxStamina : 1.f;
    float MoodPct    = Char->StateComp
        ? Char->StateComp->Mood / UCharacterStateComponent::MaxMood : 0.7f;

    HUDWidget->UpdateHpMp(
        Char->CurrentHp, Char->Stats.MaxHpBase,
        Char->CurrentMp, Char->Stats.MaxMpBase,
        StaminaPct, MoodPct);

    // ── 法術熱鍵欄 ───────────────────────────────────────────────────
    if (Char->SpellCasterComp)
    {
        const FSpellLoadout& L = Char->SpellCasterComp->SpellGroups.GetActiveLoadout();
        HUDWidget->UpdateSpellHotBar(L.Slots, L.ActiveIndex);
    }

    // ── 副手欄 + 物品熱鍵欄 + 拿取槽 ───────────────────────────────
    if (Char->InventoryComp)
    {
        HUDWidget->UpdateOffhandSlot(
            Char->InventoryComp->OffhandSlot, Char->InventoryComp->bOffhandActive);
        HUDWidget->UpdateItemHotbar(
            Char->InventoryComp->Slots, Char->InventoryComp->ActiveHotbarIndex);
    }
    {
        const bool bCarrying = Char->IsCarrying();
        EItemId CarryId = EItemId::None;
        if (bCarrying)
        {
            if (APhysicalItemActor* PhysItem = Cast<APhysicalItemActor>(Char->CarriedActor.Get()))
                CarryId = PhysItem->GetInventoryItemId();
        }
        HUDWidget->UpdateCarrySlot(bCarrying, CarryId);
    }

    // ── 加工選單（docs/plan-item-crafting-system.md §八）────────────────
    if (CraftingPanel && Char->InventoryComp)
    {
        UCraftingStationSubsystem* CraftSub = GetWorld()->GetSubsystem<UCraftingStationSubsystem>();
        const FVector PlayerLoc = Char->GetActorLocation();
        const bool bHasWorkbench = CraftSub && CraftSub->HasNearbyWorkbench(PlayerLoc);
        AChestActor* NearbyChest = CraftSub ? CraftSub->FindNearbyChest(PlayerLoc) : nullptr;
        CraftingPanel->RefreshCraftable(
            Char->InventoryComp,
            NearbyChest ? NearbyChest->InventoryComp : nullptr,
            bHasWorkbench);
        if (CraftingHintCard)
            CraftingHintCard->UpdateCount(CraftingPanel->GetCraftableCount());
    }

    // ── 生存條 ───────────────────────────────────────────────────────
    if (Char->StateComp)
        HUDWidget->UpdateSurvival(Char->StateComp);

    // ── 異常狀態圖示列（頂部中央）─────────────────────────────────────
    if (Char->SpecialStatusComp)
    {
        TArray<FStatusDisplaySnapshot> Snaps;
        Char->SpecialStatusComp->GetStatusSnapshots(Snaps);
        HUDWidget->UpdateAbnormalStatusBar(Snaps);
    }

    // ── 等級 / XP / 境界 ─────────────────────────────────────────────
    {
        FLinearColor TierCol(0.85f, 0.75f, 0.30f);
        HUDWidget->UpdateLevelHUD(
            Char->Level, Char->Xp, Char->XpRequired(Char->Level),
            Char->GetTierName(Char->Level), TierCol);
    }

    // ── 裝備標籤（2026-06-23：動態跑 FEquipmentSlotRegistry 全部欄位，不寫死武器/防具/飾品）──
    if (Char->EquipmentComp)
    {
        TArray<FString> Parts;
        for (const FEquipmentSlotDef& Def : FEquipmentSlotRegistry::GetAll())
        {
            const EItemId Eq = Char->EquipmentComp->GetEquipped(Def.Id);
            const FString Name = Eq != EItemId::None ? FItemRegistry::Get(Eq).DisplayName.ToString() : TEXT("─");
            Parts.Add(FString::Printf(TEXT("%s:%s"), *Def.DisplayName.ToString(), *Name));
        }
        HUDWidget->UpdateEquipLabel(FString::Join(Parts, TEXT("  ")));
    }

    // ── 法力條 ───────────────────────────────────────────────────────
    HUDWidget->UpdateManaSlots(Char->ActiveManaSlots);

    // ── 面板資料（若可見才刷新，避免不必要 CPU）─────────────────────
    // PlayerPanel Stats Tab（Stage 3 起 StatsContent 接上後才真正有效）
    if (PlayerPanel && PlayerPanel->GetVisibility() == ESlateVisibility::Visible)
        PlayerPanel->RefreshStatsTab(
            Char->Stats,
            Char->CurrentHp, Char->Stats.MaxHpBase,
            Char->CurrentMp, Char->Stats.MaxMpBase,
            Char->Level, Char->GetTierName(Char->Level),
            Char->EquipmentComp);
    if (InventoryPanel && InventoryPanel->GetVisibility() == ESlateVisibility::Visible)
        InventoryPanel->Refresh(Char->InventoryComp);

    if (EquipmentPanel && EquipmentPanel->GetVisibility() == ESlateVisibility::Visible)
        EquipmentPanel->Refresh(Char->EquipmentComp);

    // SpellListPanel 不在 DrawHUD 每幀刷新；RefreshSpellList 只在面板開啟和切換組別時呼叫。

    // ── S-3：鎖敵指示器（紅色方框跟隨鎖定目標）────────────────────────
    if (Char->LockedTarget && Char->LockedTarget->IsAlive())
    {
        FVector2D SP;
        const FVector TargetPos = Char->LockedTarget->GetActorLocation() + FVector(0.f, 0.f, 30.f);
        if (PC->ProjectWorldLocationToScreen(TargetPos, SP, true))
        {
            constexpr float S = 20.f;
            const FLinearColor LockCol(1.f, 0.2f, 0.2f, 0.85f);
            DrawRect(LockCol, SP.X - S,     SP.Y - S,     5.f,    S * 2.f); // 左邊
            DrawRect(LockCol, SP.X + S - 5, SP.Y - S,     5.f,    S * 2.f); // 右邊
            DrawRect(LockCol, SP.X - S,     SP.Y - S,     S * 2.f, 5.f);    // 上邊
            DrawRect(LockCol, SP.X - S,     SP.Y + S - 5, S * 2.f, 5.f);    // 下邊
        }
    }

    // ── G-6 拿取提示（附近有可撿物時在畫面中下方顯示）────────────────
    if (!Char->bIsCarrying)
    {
        if (AActor* Near = Char->FindNearestPickable())
        {
            IPhysicalPickable* P = Cast<APhysicalItemActor>(Near);
            if (!P) P = Cast<ADebrisActor>(Near);
            FString Hint = P ? P->GetPickupHintText().ToString() : TEXT("G 拿取");
            const float CanvasW = Canvas ? Canvas.Get()->SizeX : 1280.f;
            const float CanvasH = Canvas ? Canvas.Get()->SizeY : 720.f;
            DrawText(Hint, FColor::White, CanvasW * 0.5f - 60.f, CanvasH * 0.72f, GEngine->GetSmallFont(), 1.2f);
        }
    }

    // ARC-3：投擲瞄準弧線（充電期間顯示）
    DrawThrowArc();
}

// ── G-7/G-8 投擲力量條 ────────────────────────────────────────────────────

void ASkillCreatorHUD::StartThrowCharge()
{
    if (ThrowWidget)
        ThrowWidget->StartCharging();
    bIsChargingThrow = true;  // ARC-1
}

void ASkillCreatorHUD::FinishThrowCharge(ASkillCreatorCharacter* Char)
{
    bIsChargingThrow = false;  // ARC-1
    if (!ThrowWidget || !Char) return;

    ThrowWidget->StopCharging();
    const float PowerPct = ThrowWidget->GetPowerPct();
    ThrowWidget->Reset();

    if (!Char->bIsCarrying) return;

    APlayerController* PC = GetOwningPlayerController();
    FVector AimDir = PC ? PC->GetControlRotation().Vector() : Char->GetActorForwardVector();

    IPhysicalPickable* P = Cast<APhysicalItemActor>(Char->CarriedActor.Get());
    if (!P) P = Cast<ADebrisActor>(Char->CarriedActor.Get());
    const float Mass     = P ? P->GetMass() : 1.f;
    const float Strength = Char->Stats.Strength;

    constexpr float BaseSpeed = 200.f * WorldScale::TileSizeCm;
    const float Speed = FMath::Clamp(
        BaseSpeed * PowerPct * Strength / FMath::Max(0.1f, Mass),
        0.f, 5000.f);

    Char->EndCarry(AimDir * Speed);
}

// ── ARC-2：投擲瞄準弧線 ───────────────────────────────────────────────────────
// 拋體模擬（60ms 步進），DrawLine 在 HUD 螢幕空間繪製折線 + 落點十字
// 不使用 Niagara Ribbon（需 Editor .uasset），純 C++ 零 Editor 操作

void ASkillCreatorHUD::DrawThrowArc()
{
    if (!bIsChargingThrow || !ThrowWidget) return;

    APlayerController* PC = GetOwningPlayerController();
    ASkillCreatorCharacter* Char = PC ? PC->GetPawn<ASkillCreatorCharacter>() : nullptr;
    if (!Char || !Char->bIsCarrying) return;

    const float ChargePct = ThrowWidget->GetCurrentChargePct();
    if (ChargePct < 0.01f) return;  // 指針剛啟動，弧線無意義

    IPhysicalPickable* Pick = Cast<APhysicalItemActor>(Char->CarriedActor.Get());
    if (!Pick) Pick = Cast<ADebrisActor>(Char->CarriedActor.Get());
    const float Mass     = Pick ? Pick->GetMass() : 1.f;
    const float Strength = Char->Stats.Strength;

    constexpr float BaseSpeed = 200.f * WorldScale::TileSizeCm;
    const float Speed = FMath::Clamp(
        BaseSpeed * ChargePct * Strength / FMath::Max(0.1f, Mass),
        0.f, 5000.f);

    FVector AimDir = PC->GetControlRotation().Vector();
    FVector LaunchPos = Char->GetActorLocation()
        + AimDir * WorldScale::TileSizeCm * 2.f
        + FVector(0.f, 0.f, WorldScale::TileSizeCm);  // 略高於角色中心

    // 拋體模擬（UE5 Z-up：GetGravityZ() ≈ -980 cm/s²）
    const FVector Gravity(0.f, 0.f, GetWorld()->GetGravityZ());
    constexpr float Dt      = 0.06f;
    constexpr float MaxTime = 5.f;

    TArray<FVector2D> ScreenPts;
    ScreenPts.Reserve(static_cast<int32>(MaxTime / Dt) + 2);

    AVoxelWorldActor* VWA = AVoxelWorldActor::FindInWorld(GetWorld());
    FVector LastPos = LaunchPos;
    bool bLanded = false;

    for (float T = Dt; T <= MaxTime; T += Dt)
    {
        const FVector Pos = LaunchPos + AimDir * Speed * T + 0.5f * Gravity * T * T;

        // 著地判斷：查 tile（非 Air、非 Water = 固體）
        if (VWA)
        {
            const FGridPos GP = WorldScale::WorldToTile(Pos, VWA->WorldHeight);
            const EMaterialType Mat = VWA->GetTileWorld()->GetTile(GP.X, GP.Y, GP.Z);
            if (Mat != EMaterialType::Air && Mat != EMaterialType::Water)
            {
                FVector2D FinalSP;
                if (PC->ProjectWorldLocationToScreen(LastPos, FinalSP, true))
                    ScreenPts.Add(FinalSP);
                bLanded = true;
                break;
            }
        }

        FVector2D SP;
        if (PC->ProjectWorldLocationToScreen(Pos, SP, true))
            ScreenPts.Add(SP);

        LastPos = Pos;
    }

    if (ScreenPts.Num() < 2) return;

    // ── 繪製弧線折線（黃色半透明）──────────────────────────────────────
    static const FLinearColor ArcColor(1.f, 0.85f, 0.1f, 0.85f);
    for (int32 i = 0; i + 1 < ScreenPts.Num(); ++i)
    {
        DrawLine(ScreenPts[i  ].X, ScreenPts[i  ].Y,
                 ScreenPts[i+1].X, ScreenPts[i+1].Y,
                 ArcColor, 2.f);
    }

    // ── 落點十字標記（橘紅色）────────────────────────────────────────────
    if (bLanded)
    {
        const FVector2D& Landing = ScreenPts.Last();
        static const FLinearColor LandColor(1.f, 0.3f, 0.1f, 1.f);
        constexpr float CR = 6.f;
        DrawLine(Landing.X - CR, Landing.Y,      Landing.X + CR, Landing.Y,      LandColor, 2.f);
        DrawLine(Landing.X,      Landing.Y - CR, Landing.X,      Landing.Y + CR, LandColor, 2.f);
    }
}
