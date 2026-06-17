#include "ASkillCreatorHUD.h"
#include "ASkillCreatorCharacter.h"
#include "USpellCaster.h"
#include "UCharacterStateComponent.h"
#include "UInventoryComponent.h"
#include "UEquipmentComponent.h"
#include "USettingsWidget.h"
#include "UShapeMenuWidget.h"
#include "USpellGroupWidget.h"
#include "UStatsWidget.h"
#include "UInventoryWidget.h"
#include "UEquipmentWidget.h"
#include "ItemRegistry.h"
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

void ASkillCreatorHUD::ToggleSettings()
{
    if (SettingsPanel)
        SettingsPanel->SyncState(bHoldToPlace, bPerfectRemove);
    TogglePanel(SettingsPanel);
}

void ASkillCreatorHUD::ToggleShapeMenu()   { TogglePanel(ShapeMenuPanel);  }
void ASkillCreatorHUD::ToggleSpellGroup()  { TogglePanel(SpellGroupPanel); }
void ASkillCreatorHUD::ToggleStats()       { TogglePanel(StatsPanel);      }
void ASkillCreatorHUD::ToggleInventory()   { TogglePanel(InventoryPanel);  }
void ASkillCreatorHUD::ToggleEquipment()   { TogglePanel(EquipmentPanel);  }

// ── BeginPlay：建立所有 widget + 綁定 delegate ───────────────────────────

void ASkillCreatorHUD::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PC = GetOwningPlayerController();
    if (!PC) return;

    // 常駐 HUD
    if (HUDWidgetClass)
    {
        HUDWidget = CreateWidget<UPlayerHUDWidget>(PC, HUDWidgetClass);
        if (HUDWidget) HUDWidget->AddToViewport();
    }

    // ── 面板 widget ────────────────────────────────────────────────────

    // Settings
    SettingsPanel = CreatePanel<USettingsWidget>();
    if (SettingsPanel)
    {
        SettingsPanel->OnHoldToPlaceChanged.BindLambda(
            [this](bool b){ bHoldToPlace = b; });
        SettingsPanel->OnPerfectRemoveChanged.BindLambda(
            [this](bool b){ bPerfectRemove = b; });
        SettingsPanel->OnCloseRequested.BindLambda(
            [this](){ if (SettingsPanel) SettingsPanel->SetVisibility(ESlateVisibility::Collapsed); });
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

    // SpellGroup
    SpellGroupPanel = CreatePanel<USpellGroupWidget>();
    if (SpellGroupPanel)
    {
        SpellGroupPanel->OnGroupSelected.BindLambda(
            [this](int32 G)
            {
                ASkillCreatorCharacter* Char = GetOwningPlayerController()
                    ? Cast<ASkillCreatorCharacter>(GetOwningPlayerController()->GetPawn()) : nullptr;
                if (Char && Char->SpellCasterComp)
                    Char->SpellCasterComp->SpellGroups.SetActiveGroup(G);
                if (SpellGroupPanel)
                    SpellGroupPanel->SetVisibility(ESlateVisibility::Collapsed);
            });
        SpellGroupPanel->OnCloseRequested.BindLambda(
            [this](){ if (SpellGroupPanel) SpellGroupPanel->SetVisibility(ESlateVisibility::Collapsed); });
    }

    // Stats
    StatsPanel = CreatePanel<UStatsWidget>();

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
                if (Char && Char->InventoryComp && Char->EquipComp)
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
            [this](EEquipmentSlotType Slot)
            {
                ASkillCreatorCharacter* Char = GetOwningPlayerController()
                    ? Cast<ASkillCreatorCharacter>(GetOwningPlayerController()->GetPawn()) : nullptr;
                if (Char && Char->EquipComp && Char->InventoryComp)
                {
                    Char->EquipmentComp->TryUnequip(Char->InventoryComp, Slot);
                    EquipmentPanel->Refresh(Char->EquipmentComp);
                    if (InventoryPanel) InventoryPanel->Refresh(Char->InventoryComp);
                }
            });
    }
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

    // ── 物品熱鍵欄 ───────────────────────────────────────────────────
    if (Char->InventoryComp)
        HUDWidget->UpdateItemHotbar(
            Char->InventoryComp->Slots, Char->InventoryComp->ActiveHotbarIndex);

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

    // ── 裝備標籤 ─────────────────────────────────────────────────────
    if (Char->EquipmentComp)
    {
        auto ItemName = [](EItemId Id) -> FString
        {
            return Id != EItemId::None
                ? FItemRegistry::Get(Id).DisplayName.ToString()
                : TEXT("─");
        };
        HUDWidget->UpdateEquipLabel(
            ItemName(Char->EquipmentComp->WeaponId),
            ItemName(Char->EquipmentComp->ArmorId),
            ItemName(Char->EquipmentComp->AccessoryId));
    }

    // ── 法力條 ───────────────────────────────────────────────────────
    HUDWidget->UpdateManaSlots(Char->ActiveManaSlots);

    // ── 面板資料（若可見才刷新，避免不必要 CPU）─────────────────────
    if (StatsPanel && StatsPanel->GetVisibility() == ESlateVisibility::Visible)
        StatsPanel->Refresh(Char->Stats,
            Char->CurrentHp, Char->Stats.MaxHpBase,
            Char->CurrentMp, Char->Stats.MaxMpBase,
            Char->Level, Char->GetTierName(Char->Level),
            Char->EquipmentComp);

    if (InventoryPanel && InventoryPanel->GetVisibility() == ESlateVisibility::Visible)
        InventoryPanel->Refresh(Char->InventoryComp);

    if (EquipmentPanel && EquipmentPanel->GetVisibility() == ESlateVisibility::Visible)
        EquipmentPanel->Refresh(Char->EquipmentComp);
}
