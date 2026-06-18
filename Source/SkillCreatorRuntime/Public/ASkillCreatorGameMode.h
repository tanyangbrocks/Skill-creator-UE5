#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WorldSaveData.h"
#include "CharacterSaveData.h"
#include "ASkillCreatorGameMode.generated.h"

class UGameFlowWidget;

// 預設 GameMode：綁定 ASkillCreatorCharacter / ASkillCreatorHUD /
// ASkillCreatorPlayerController，讓關卡不需要手動在 Editor 設定。
// DefaultGame.ini 已將此類設為 GlobalDefaultGameMode。
//
// 單一地圖（MainMap）兼當選單與遊戲關卡：
//   BeginPlay 時若 UGameSessionSubsystem 沒有 PendingWorld+PendingCharacter
//   -> 顯示 UGameFlowWidget 讓玩家選/建角色、選/建世界
//   玩家完成選擇 -> UGameFlowWidget::OnEnterGame -> StartGameplayWithWorld -> 收起 UI、正式開局
UCLASS()
class SKILLCREATORRUNTIME_API ASkillCreatorGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASkillCreatorGameMode();
    virtual void BeginPlay() override;

    // UGameFlowWidget 選好角色+世界後呼叫，正式進入遊戲（同一張地圖，不換關卡）
    void StartGameplayWithWorld(const FWorldSaveData& World, const FCharacterSaveData& Character);

private:
    UPROPERTY()
    TObjectPtr<UGameFlowWidget> GameFlowWidget;

    void SpawnWorldAndMobs(int32 WorldSeed);
};
