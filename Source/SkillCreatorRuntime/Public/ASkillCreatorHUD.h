#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UPlayerHUDWidget.h"
#include "ASkillCreatorHUD.generated.h"

// Manages the player HUD widget lifecycle and per-frame data feed.
//
// Setup in Editor:
//   1. Set HUDWidgetClass to your WBP_PlayerHUD (derived from UPlayerHUDWidget).
//   2. Assign this class as the GameMode's HUD Class.
UCLASS()
class SKILLCREATORRUNTIME_API ASkillCreatorHUD : public AHUD
{
    GENERATED_BODY()
public:
    ASkillCreatorHUD();

    // Assign in Editor (GameMode or Blueprint): Widget Blueprint derived from
    // UPlayerHUDWidget that contains the visual HP/MP bar layout.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD")
    TSubclassOf<UPlayerHUDWidget> HUDWidgetClass;

    UPROPERTY(BlueprintReadOnly, Category="HUD")
    TObjectPtr<UPlayerHUDWidget> HUDWidget;

    virtual void BeginPlay() override;

    // Called every frame before rendering; pushes data from the character
    // to the widget's BlueprintReadOnly properties.
    virtual void DrawHUD() override;
};
