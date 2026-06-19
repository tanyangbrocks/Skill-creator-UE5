#include "ASkillCreatorGameMode.h"
#include "ASkillCreatorCharacter.h"
#include "ASkillCreatorHUD.h"
#include "ASkillCreatorPlayerController.h"
#include "AVoxelWorldActor.h"
#include "AMobSpawnController.h"
#include "AEnemy.h"
#include "AEnemyManager.h"
#include "EngineUtils.h"
#include "UGameSessionSubsystem.h"
#include "UGameFlowWidget.h"
#include "Blueprint/UserWidget.h"
#include "FlowSaveSystem.h"
#include "TimerManager.h"

ASkillCreatorGameMode::ASkillCreatorGameMode()
{
    DefaultPawnClass       = ASkillCreatorCharacter::StaticClass();
    HUDClass                = ASkillCreatorHUD::StaticClass();
    PlayerControllerClass  = ASkillCreatorPlayerController::StaticClass();
}

void ASkillCreatorGameMode::BeginPlay()
{
    Super::BeginPlay();

    UGameSessionSubsystem* Session = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UGameSessionSubsystem>() : nullptr;

    if (Session && Session->HasPendingWorld() && Session->HasPendingCharacter())
    {
        // 已經透過 UGameFlowWidget 選好角色+世界（玩家死亡重生 / PIE 重跑等情況也會在這裡直接開局）
        UE_LOG(LogTemp, Log, TEXT("SkillCreatorGameMode: pending world+character found, starting gameplay directly"));
        StartGameplayWithWorld(Session->GetPendingWorld(), Session->GetPendingCharacter());
        return;
    }

    // 還沒選角色/世界：顯示 UGameFlowWidget，暫停一般 gameplay，等玩家選/建角色、選/建世界
    UE_LOG(LogTemp, Log, TEXT("SkillCreatorGameMode: no pending world/character — showing GameFlowWidget"));
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC)
    {
        GameFlowWidget = CreateWidget<UGameFlowWidget>(PC, UGameFlowWidget::StaticClass());
        if (GameFlowWidget)
        {
            GameFlowWidget->AddToViewport(100);
            // 確保 viewport slot 真的撐滿整個畫面（不依賴 AddToViewport 的隱含預設行為）。
            GameFlowWidget->SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
            GameFlowWidget->SetAlignmentInViewport(FVector2D(0.f, 0.f));
            FInputModeUIOnly InputMode;
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(InputMode);
            PC->SetShowMouseCursor(true);
            PC->SetPause(true);
            UE_LOG(LogTemp, Log, TEXT("SkillCreatorGameMode: GameFlowWidget created, added to viewport, game paused"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("SkillCreatorGameMode: CreateWidget<UGameFlowWidget> FAILED"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SkillCreatorGameMode: GetFirstPlayerController() returned null, cannot show GameFlowWidget"));
    }
}

void ASkillCreatorGameMode::StartGameplayWithWorld(const FWorldSaveData& World, const FCharacterSaveData& Character)
{
    if (GameFlowWidget)
    {
        GameFlowWidget->RemoveFromParent();
        GameFlowWidget = nullptr;
    }

    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->SetShowMouseCursor(false);
        PC->SetPause(false);
    }

    CurrentWorldSave     = World;
    CurrentCharacterSave = Character;
    bGameplayStarted     = true;

    SpawnWorldAndMobs(World.Seed);

    // 角色在 BeginPlay 當下（選世界之前）就已經 spawn 了，
    // 那時 AVoxelWorldActor 還不存在，CachedVoxelWorld 會是 null，必須在這裡補一次重新綁定，
    // 同時套用玩家選定角色的存檔進度（取代 BeginPlay 當下灌入的預設值）
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        if (ASkillCreatorCharacter* Char = Cast<ASkillCreatorCharacter>(PC->GetPawn()))
        {
            Char->RebindWorldSystems();
            Char->ApplyCharacterSaveData(Character);

            Char->OnCharacterDied.AddDynamic(this, &ASkillCreatorGameMode::PerformSave);
        }

    // 每 30 秒自動存檔
    GetWorldTimerManager().SetTimer(
        PeriodicSaveHandle,
        this,
        &ASkillCreatorGameMode::PerformSave,
        30.f,
        /*bLoop=*/true);
}

void ASkillCreatorGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(PeriodicSaveHandle);

    if (bGameplayStarted)
        PerformSave();

    Super::EndPlay(EndPlayReason);
}

void ASkillCreatorGameMode::PerformSave()
{
    if (!bGameplayStarted) return;

    // 更新角色執行期狀態進存檔結構
    if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
        if (ASkillCreatorCharacter* Char = Cast<ASkillCreatorCharacter>(PC->GetPawn()))
            Char->FillSaveData(CurrentCharacterSave);

    FFlowSaveSystem::SaveCharacter(CurrentCharacterSave);

    if (AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(GetWorld()))
        FFlowSaveSystem::SaveAll(*VW->GetTileWorld(), CurrentWorldSave);
}

void ASkillCreatorGameMode::SpawnWorldAndMobs(int32 WorldSeed)
{
    // 若 MainMap 裡沒有手動放置的 AVoxelWorldActor，自動生成一個（用選定的 Seed）
    AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(GetWorld());
    if (!VW)
    {
        const FTransform T = FTransform::Identity;
        VW = GetWorld()->SpawnActorDeferred<AVoxelWorldActor>(AVoxelWorldActor::StaticClass(), T);
        if (VW)
        {
            VW->WorldSeed = WorldSeed;
            VW->FinishSpawning(T);
        }
    }

    // 自動生成 AEnemyManager（若關卡中沒有手動放置）。必須在 AMobSpawnController 之前
    // 生成 —— AMobSpawnController::BeginPlay() 用 TActorIterator 找 AEnemyManager，
    // 順序顛倒會導致一直找不到，敵人系統整個不會啟動（2026-06-19 稽核發現：先前完全沒有
    // 任何程式碼會生成 AEnemyManager，除非關卡裡手動放置，否則敵人完全不會出現）。
    bool bHasEnemyMgr = false;
    for (TActorIterator<AEnemyManager> It(GetWorld()); It; ++It)
    { bHasEnemyMgr = true; break; }

    if (!bHasEnemyMgr)
        GetWorld()->SpawnActor<AEnemyManager>();

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
