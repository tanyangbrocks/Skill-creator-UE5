#include "ASkillCreatorHUD.h"
#include "ASkillCreatorCharacter.h"
#include "USpellCaster.h"
#include "UCharacterStateComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

ASkillCreatorHUD::ASkillCreatorHUD()
{
    // 直接用 C++ 類別，不需要 WBP_PlayerHUD Blueprint
    HUDWidgetClass = UPlayerHUDWidget::StaticClass();
}

void ASkillCreatorHUD::BeginPlay()
{
    Super::BeginPlay();

    if (!HUDWidgetClass) return;

    HUDWidget = CreateWidget<UPlayerHUDWidget>(GetOwningPlayerController(), HUDWidgetClass);
    if (HUDWidget)
        HUDWidget->AddToViewport();
}

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
        ? Char->StateComp->Stamina / UCharacterStateComponent::MaxStamina
        : 1.f;
    float MoodPct    = Char->StateComp
        ? Char->StateComp->Mood / UCharacterStateComponent::MaxMood
        : 0.7f;

    HUDWidget->UpdateHpMp(
        Char->CurrentHp, Char->Stats.MaxHpBase,
        Char->CurrentMp, Char->Stats.MaxMpBase,
        StaminaPct, MoodPct);

    // ── Spell hotbar ──────────────────────────────────────────────────
    if (Char->SpellCasterComp)
    {
        const FSpellLoadout& L = Char->SpellCasterComp->SpellGroups.GetActiveLoadout();
        HUDWidget->UpdateSpellHotBar(L.Slots, L.ActiveIndex);
    }
}
