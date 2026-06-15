#include "ASkillCreatorPlayerController.h"
#include "ASkillCreatorCharacter.h"
#include "USpellCaster.h"
#include "GameFramework/Pawn.h"

void ASkillCreatorPlayerController::BeginPlay()
{
    Super::BeginPlay();
    // TODO M-5: AddMappingContext(DefaultMappingContext, 0)
}

void ASkillCreatorPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    // Scroll-wheel slot cycling (legacy BindKey — no Input.ini entry needed).
    InputComponent->BindKey(EKeys::MouseScrollUp,   IE_Pressed, this, &ASkillCreatorPlayerController::OnScrollUp);
    InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ASkillCreatorPlayerController::OnScrollDown);
    // TODO M-5: 綁定 EnhancedInput 動作到施法 / 技能切換
}

void ASkillCreatorPlayerController::SetActiveSpellSlot(int32 Idx)
{
    ASkillCreatorCharacter* Char =
        GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char && Char->SpellCasterComp)
        Char->SpellCasterComp->SwitchSlot(Idx);
}

void ASkillCreatorPlayerController::OnScrollUp()
{
    ASkillCreatorCharacter* Char =
        GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char && Char->SpellCasterComp)
        Char->SpellCasterComp->CycleSlot(+1);
}

void ASkillCreatorPlayerController::OnScrollDown()
{
    ASkillCreatorCharacter* Char =
        GetPawn() ? Cast<ASkillCreatorCharacter>(GetPawn()) : nullptr;
    if (Char && Char->SpellCasterComp)
        Char->SpellCasterComp->CycleSlot(-1);
}
