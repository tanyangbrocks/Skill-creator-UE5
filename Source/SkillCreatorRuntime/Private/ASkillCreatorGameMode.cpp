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
#include "WorldScale.h"
#include "GridPos.h"

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
            // viewport slot 設成 anchor fill(0,0→1,1) + offset 全零 → slot 等於整個螢幕，
            // Widget 根節點 UBorder 填滿 slot 即完整覆蓋畫面。
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

    SpawnWorldAndMobs(World.Seed, World.Id);

    AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(GetWorld());

    // Godot GameFlowUI.cs:451-502（PregenerateWorld）/ Main.cs:284-291：
    // 新世界第一次進入時，預先生成出生點周圍地形、寫入磁碟、記錄出生點，
    // 之後永遠走「磁碟讀取 / Streaming 懶生成」路徑，不再重算。
    // 自 2026-06-20 起，UGameFlowWidget 建立世界時已經會立即預生成（對齊 Godot 行為），
    // 所以這裡通常不會再觸發；保留作為防呆（例如世界目錄手動複製、bIsFirstEnter 殘留 true 的情況）。
    FIntVector WorldSpawnTile = CurrentWorldSave.PlayerSpawn;
    if (CurrentWorldSave.bIsFirstEnter && VW)
    {
        FFlowSaveSystem::PregenerateSpawnArea(*VW->GetTileWorld(), VW->GetMapGenerator(), CurrentWorldSave);
        WorldSpawnTile = CurrentWorldSave.PlayerSpawn;
    }

    // 角色在 BeginPlay 當下（選世界之前）就已經 spawn 了，
    // 那時 AVoxelWorldActor 還不存在，CachedVoxelWorld 會是 null，必須在這裡補一次重新綁定，
    // 同時套用玩家選定角色的存檔進度（取代 BeginPlay 當下灌入的預設值）
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        if (ASkillCreatorCharacter* Char = Cast<ASkillCreatorCharacter>(PC->GetPawn()))
        {
            Char->RebindWorldSystems();
            Char->ApplyCharacterSaveData(Character);

            // 角色自己的存檔位置優先（回流玩家，FillSaveData 寫入的 TilePosition）；
            // 沒有（新角色 / 新世界）則退回世界出生點，避免一律落在關卡 PlayerStart。
            if (VW)
            {
                const FIntVector PlacementTile =
                    (Character.TilePosition != FIntVector::ZeroValue) ? Character.TilePosition : WorldSpawnTile;
                Char->SetActorLocation(WorldScale::TileToWorld(
                    FGridPos(PlacementTile.X, PlacementTile.Y, PlacementTile.Z), VW->WorldHeight));
            }

            Char->OnCharacterDied.AddDynamic(this, &ASkillCreatorGameMode::PerformSave);
        }

    // 2026-06-22 診斷用：使用者回報熱鍵欄/準心/採掘/敵人全部消失，懷疑是 StartGameplayWithWorld()
    // 沒有真正跑到、或跑到但某個環節綁定失敗。這裡印出關鍵狀態，下次測試時直接看 log 就能
    // 確認問題出在哪一段，不用再憑空猜——找到根因後這段 log 就可以移除。
    {
        int32 EnemyMgrCount = 0, SpawnCtrlCount = 0;
        for (TActorIterator<AEnemyManager> It(GetWorld()); It; ++It) ++EnemyMgrCount;
        for (TActorIterator<AMobSpawnController> It(GetWorld()); It; ++It) ++SpawnCtrlCount;
        APlayerController* DiagPC = GetWorld()->GetFirstPlayerController();
        ASkillCreatorCharacter* DiagChar = DiagPC ? Cast<ASkillCreatorCharacter>(DiagPC->GetPawn()) : nullptr;
        ASkillCreatorHUD* DiagHUD = DiagPC ? Cast<ASkillCreatorHUD>(DiagPC->GetHUD()) : nullptr;
        UE_LOG(LogTemp, Warning,
            TEXT("[DIAG] StartGameplayWithWorld 結束：VW=%s, PC=%s, Char=%s, ")
            TEXT("HUD=%s, HUD->HUDWidget=%s, EnemyManagerCount=%d, MobSpawnControllerCount=%d"),
            VW ? TEXT("valid") : TEXT("NULL"),
            DiagPC ? TEXT("valid") : TEXT("NULL"),
            DiagChar ? TEXT("valid") : TEXT("NULL"),
            DiagHUD ? TEXT("valid") : TEXT("NULL"),
            (DiagHUD && DiagHUD->HUDWidget) ? TEXT("valid") : TEXT("NULL"),
            EnemyMgrCount, SpawnCtrlCount);
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

void ASkillCreatorGameMode::SpawnWorldAndMobs(int32 WorldSeed, const FString& WorldId)
{
    // 若 MainMap 裡沒有手動放置的 AVoxelWorldActor，自動生成一個（用選定的 Seed）
    AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(GetWorld());
    if (!VW)
    {
        const FTransform T = FTransform::Identity;
        VW = GetWorld()->SpawnActorDeferred<AVoxelWorldActor>(AVoxelWorldActor::StaticClass(), T);
        if (VW)
        {
            VW->WorldSeed     = WorldSeed;
            // 修正既有缺口：WorldSaveDir 從未從玩家選定的世界目錄設定，永遠用 class default
            // "World_0001"，導致多世界的 chunk streaming 全部讀寫同一個資料夾。
            // FFlowSaveSystem::WorldRoot(Id) == ProjectSaved/Worlds/{Id}，與 WorldId 同義。
            VW->WorldSaveDir  = WorldId;
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
