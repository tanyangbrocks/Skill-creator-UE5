#include "ASkillCreatorGameMode.h"
#include "ASkillCreatorCharacter.h"
#include "ASkillCreatorHUD.h"
#include "ASkillCreatorPlayerController.h"

ASkillCreatorGameMode::ASkillCreatorGameMode()
{
    DefaultPawnClass       = ASkillCreatorCharacter::StaticClass();
    HUDClass               = ASkillCreatorHUD::StaticClass();
    PlayerControllerClass  = ASkillCreatorPlayerController::StaticClass();
}
