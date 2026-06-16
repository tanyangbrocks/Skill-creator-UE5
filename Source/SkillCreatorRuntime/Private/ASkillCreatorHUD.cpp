#include "ASkillCreatorHUD.h"
#include "ASkillCreatorCharacter.h"
#include "USpellCaster.h"
#include "UCharacterStateComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

ASkillCreatorHUD::ASkillCreatorHUD()
{
    static ConstructorHelpers::FClassFinder<UPlayerHUDWidget> WidgetFinder(TEXT("/Game/WBP_PlayerHUD"));
    if (WidgetFinder.Succeeded())
        HUDWidgetClass = WidgetFinder.Class;
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
