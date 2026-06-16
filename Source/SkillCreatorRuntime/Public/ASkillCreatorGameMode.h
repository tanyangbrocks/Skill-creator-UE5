#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ASkillCreatorGameMode.generated.h"

// 預設 GameMode：綁定 ASkillCreatorCharacter / ASkillCreatorHUD /
// ASkillCreatorPlayerController，讓關卡不需要手動在 Editor 設定。
// DefaultGame.ini 已將此類設為 GlobalDefaultGameMode。
UCLASS()
class SKILLCREATORRUNTIME_API ASkillCreatorGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASkillCreatorGameMode();
    virtual void BeginPlay() override;
};
