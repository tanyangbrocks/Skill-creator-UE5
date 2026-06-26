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
#include "IPhysicalPickable.h"
#include "ADebrisActor.h"
#include "UPhysicalThrowWidget.h"
#include "WorldScale.h"
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
            PC->SetInputMode(FInputModeGameAndUI());
        }
    }
    else
    {
        PlayerPanel->SetVisibility(ESlateVisibility::Collapsed);
        if (APlayerController* PC = GetOwningPlayerController())
        {
            PC->SetShowMouseCursor(false);
            PC->SetInputMode(FInputModeGameAndUI());
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

    // 加工選單提示圖卡（常駐左側，不按鍵開關）
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

    // ── 副手欄 + 物品熱鍵欄 ──────────────────────────────────────────
    if (Char->InventoryComp)
    {
        HUDWidget->UpdateOffhandSlot(
            Char->InventoryComp->OffhandSlot, Char->InventoryComp->bOffhandActive);
        HUDWidget->UpdateItemHotbar(
            Char->InventoryComp->Slots, Char->InventoryComp->ActiveHotbarIndex);
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
}

// ── G-7/G-8 投擲力量條 ────────────────────────────────────────────────────

void ASkillCreatorHUD::StartThrowCharge()
{
    if (ThrowWidget)
        ThrowWidget->StartCharging();
}

void ASkillCreatorHUD::FinishThrowCharge(ASkillCreatorCharacter* Char)
{
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
