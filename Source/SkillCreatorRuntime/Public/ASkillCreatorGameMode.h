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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // UGameFlowWidget 選好角色+世界後呼叫，正式進入遊戲（同一張地圖，不換關卡）
    void StartGameplayWithWorld(const FWorldSaveData& World, const FCharacterSaveData& Character);

private:
    UPROPERTY()
    TObjectPtr<UGameFlowWidget> GameFlowWidget;

    // 遊玩中的當前角色與世界存檔 metadata（Id/Name/WorldDir/Seed 等）
    FWorldSaveData     CurrentWorldSave;
    FCharacterSaveData CurrentCharacterSave;
    bool               bGameplayStarted = false;

    FTimerHandle PeriodicSaveHandle;

    // 把目前遊戲狀態寫回磁碟（角色進度 + 世界 dirty chunks）
    // UFUNCTION 供 OnCharacterDied（DYNAMIC_MULTICAST）AddDynamic 綁定
    UFUNCTION()
    void PerformSave();

    void SpawnWorldAndMobs(int32 WorldSeed);
};
