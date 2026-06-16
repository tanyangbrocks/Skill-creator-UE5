#include "ASkillCreatorPlayerController.h"
#include "ASkillCreatorCharacter.h"
#include "USpellCaster.h"
#include "GameFramework/Pawn.h"
#if WITH_EDITOR
#include "Framework/Docking/TabManager.h"
#endif

void ASkillCreatorPlayerController::BeginPlay()
{
    Super::BeginPlay();
    // TODO M-5: AddMappingContext(DefaultMappingContext, 0)
}

void ASkillCreatorPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    // 滑鼠滾輪與功能鍵用 BindKey（直接按鍵，不走 ActionMappings 查表）
    InputComponent->BindKey(EKeys::MouseScrollUp,   IE_Pressed, this, &ASkillCreatorPlayerController::OnScrollUp);
    InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ASkillCreatorPlayerController::OnScrollDown);
    InputComponent->BindKey(EKeys::E,               IE_Pressed, this, &ASkillCreatorPlayerController::OnOpenEditor);
    InputComponent->BindKey(EKeys::One,             IE_Pressed, this, &ASkillCreatorPlayerController::OnHotbar1);
    InputComponent->BindKey(EKeys::Two,             IE_Pressed, this, &ASkillCreatorPlayerController::OnHotbar2);
    InputComponent->BindKey(EKeys::Three,           IE_Pressed, this, &ASkillCreatorPlayerController::OnHotbar3);
    InputComponent->BindKey(EKeys::Four,            IE_Pressed, this, &ASkillCreatorPlayerController::OnHotbar4);
    InputComponent->BindKey(EKeys::Five,            IE_Pressed, this, &ASkillCreatorPlayerController::OnHotbar5);
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

void ASkillCreatorPlayerController::OnOpenEditor()
{
#if WITH_EDITOR
    FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("BlockSpellEditor")));
#endif
}

void ASkillCreatorPlayerController::OnHotbar1() { SetActiveSpellSlot(0); }
void ASkillCreatorPlayerController::OnHotbar2() { SetActiveSpellSlot(1); }
void ASkillCreatorPlayerController::OnHotbar3() { SetActiveSpellSlot(2); }
void ASkillCreatorPlayerController::OnHotbar4() { SetActiveSpellSlot(3); }
void ASkillCreatorPlayerController::OnHotbar5() { SetActiveSpellSlot(4); }
