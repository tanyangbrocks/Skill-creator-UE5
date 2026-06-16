#include "ASkillCreatorGameMode.h"
#include "ASkillCreatorCharacter.h"
#include "ASkillCreatorHUD.h"
#include "ASkillCreatorPlayerController.h"
#include "AVoxelWorldActor.h"
#include "AMobSpawnController.h"
#include "AEnemy.h"
#include "EngineUtils.h"

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

    // 自動生成 AMobSpawnController（若關卡中沒有手動放置）
    bool bHasSpawnCtrl = false;
    for (TActorIterator<AMobSpawnController> It(GetWorld()); It; ++It)
    { bHasSpawnCtrl = true; break; }

    if (!bHasSpawnCtrl)
    {
        AMobSpawnController* SpawnCtrl = GetWorld()->SpawnActor<AMobSpawnController>();
        if (SpawnCtrl)
        {
            FMobTableEntry Melee;
            Melee.Type     = EEnemyType::Melee;
            Melee.Category = ESpawnCategory::Common;
            Melee.Weight   = 1.f;
            SpawnCtrl->MobTable.Add(Melee);

            FMobTableEntry Ranged;
            Ranged.Type     = EEnemyType::Ranged;
            Ranged.Category = ESpawnCategory::Common;
            Ranged.Weight   = 0.5f;
            SpawnCtrl->MobTable.Add(Ranged);
        }
    }
}
