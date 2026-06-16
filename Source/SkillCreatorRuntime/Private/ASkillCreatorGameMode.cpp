#include "ASkillCreatorGameMode.h"
#include "ASkillCreatorCharacter.h"
#include "ASkillCreatorHUD.h"
#include "ASkillCreatorPlayerController.h"
#include "AVoxelWorldActor.h"

ASkillCreatorGameMode::ASkillCreatorGameMode()
{
    DefaultPawnClass       = ASkillCreatorCharacter::StaticClass();
    HUDClass               = ASkillCreatorHUD::StaticClass();
    PlayerControllerClass  = ASkillCreatorPlayerController::StaticClass();
}

void ASkillCreatorGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 若 MainMap 裡沒有手動放置的 AVoxelWorldActor，自動生成一個
    if (!AVoxelWorldActor::FindInWorld(GetWorld()))
        GetWorld()->SpawnActor<AVoxelWorldActor>(AVoxelWorldActor::StaticClass());
}
