#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanelSlot.h"
#include "SpellArray.h"
#include "UPlayerHUDWidget.generated.h"

// C++ backing class for the player HUD Widget Blueprint.
//
// Usage:
//   1. Create a Widget Blueprint (WBP_PlayerHUD) derived from this class.
//   2. In the Blueprint layout, bind ProgressBars / TextBlocks to the
//      BlueprintReadOnly properties below.
//   3. Assign WBP_PlayerHUD to ASkillCreatorHUD::HUDWidgetClass in the
//      GameMode Blueprint.
//
// Data is pushed from ASkillCreatorHUD::DrawHUD() each frame via the
// Update* methods — no polling needed in Blueprint.
UCLASS(Abstract, BlueprintType, Blueprintable)
class SKILLCREATORRUNTIME_API UPlayerHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ── HP ────────────────────────────────────────────────────────
    // 0..1 — bind directly to a ProgressBar.Percent
    UPROPERTY(BlueprintReadOnly, Category="HUD|HP")
    float HpPercent = 1.f;

    // "95 / 100" — bind to a TextBlock
    UPROPERTY(BlueprintReadOnly, Category="HUD|HP")
    FText HpText;

    // ── MP ────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, Category="HUD|MP")
    float MpPercent = 1.f;

    UPROPERTY(BlueprintReadOnly, Category="HUD|MP")
    FText MpText;

    // ── Stamina / Mood ────────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, Category="HUD|State")
    float StaminaPercent = 1.f;

    UPROPERTY(BlueprintReadOnly, Category="HUD|State")
    float MoodPercent = 0.7f;

    // ── Spell hotbar (up to 10 slots) ─────────────────────────────
    // Display name of each spell in the hotbar (empty slot = "—").
    UPROPERTY(BlueprintReadOnly, Category="HUD|Spells")
    TArray<FText> HotBarNames;

    // Index (0-based) of the currently active spell slot.
    UPROPERTY(BlueprintReadOnly, Category="HUD|Spells")
    int32 ActiveSpellIndex = 0;

    // Factor-slot list for the active spell: totem IDs / names.
    // Displayed as a simple ListView in the HUD.
    UPROPERTY(BlueprintReadOnly, Category="HUD|Spells")
    TArray<FText> ActiveSlotList;

    // ── Widget references (bound by name to Blueprint variables) ──
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar>  HpBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar>  MpBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar>  StaminaBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar>  MoodBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock>    HpTextBlock;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock>    MpTextBlock;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox>  HotBarBox;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox>  ActiveSlotBox;

    // ── C++ update API (called by ASkillCreatorHUD each frame) ────

    // Feed HP/MP/Stamina/Mood values; computes Percent and Text fields.
    void UpdateHpMp(float Hp, float MaxHp, float Mp, float MaxMp,
                    float StaminaPct, float MoodPct);

    // Rebuild hotbar names and active-slot list from the SpellCaster hotbar.
    void UpdateSpellHotBar(const TArray<FSpellArray>& HotBar, int32 ActiveIdx);

    // ── Blueprint events (implement in Widget Blueprint) ──────────

    // Called when the active spell slot changes.
    UFUNCTION(BlueprintImplementableEvent, Category="HUD|Spells")
    void OnActiveSlotChanged(int32 NewSlot);

protected:
    virtual void NativeConstruct() override;
};
