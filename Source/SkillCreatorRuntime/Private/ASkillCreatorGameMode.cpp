#include "ASkillCreatorGameMode.h"
#include "UGameClockSubsystem.h"
#include "ASkillCreatorCharacter.h"
#include "ASkillCreatorHUD.h"
#include "ASkillCreatorPlayerController.h"
#include "AVoxelWorldActor.h"
#include "AMobSpawnController.h"
#include "AEnemy.h"
#include "AEnemyManager.h"
#include "UDroppedItemManager.h"
#include "UCraftingStationSubsystem.h"
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

    // 載入此世界的累積時間（新世界 = 0，舊世界 = 上次存檔的 ElapsedTicks）
    if (UGameClockSubsystem* Clock =
        GetGameInstance()->GetSubsystem<UGameClockSubsystem>())
    {
        Clock->SetTicks(World.ElapsedTicks);
        Clock->ResumeClock();
    }

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
            // 2026-06-23 修復：角色設計上可以任意搭配世界進場（CharacterSaveData.h 頂部
            // 註解），但 TilePosition 只在「上次存檔當下的那個世界」裡才有意義——同一組
            // 座標換到另一個 seed 不同的世界，可能落在地底深處或半空中，跟那個世界真正的
            // 出生點完全無關。原本只檢查「TilePosition 非零」就直接信任，沒檢查是不是同一個
            // 世界，導致「角色 A 玩過世界 1，之後選同一個角色進全新的世界 2」這個完全正常
            // 的操作會把角色傳到世界 2 裡毫無意義的座標（不是世界 2 的出生點附近，可能根本
            // 不在地表），使用者回報「草地沒出現/不能採掘」很可能就是這個原因——不是地表
            // 生成或採掘邏輯本身有問題，是站錯地方。加上 LastWorldId 比對，只有「上次存檔
            // 的世界」跟「這次要進的世界」是同一個才信任 TilePosition。
            if (VW)
            {
                const bool bSameWorldAsLastSave =
                    !Character.LastWorldId.IsEmpty() && Character.LastWorldId == World.Id;
                const FIntVector PlacementTile =
                    (bSameWorldAsLastSave && Character.TilePosition != FIntVector::ZeroValue)
                    ? Character.TilePosition : WorldSpawnTile;

                // 2026-06-23 修復：WorldScale::TileToWorld(Y) 算出的是該 tile 的「頂面」世界座標
                // （對照 GreedyMesher.cpp:144 face_d = (facing==1 ? s+1 : s)*S 反推），但
                // SetActorLocation() 移動的是 ACharacter 膠囊體的「中心」（UE5 標準慣例，
                // 膠囊體是 RootComponent，原點＝幾何中心，不是腳底）。PlacementTile（出生點/
                // 存檔位置）一律是地表正上方那格空氣，原本直接把膠囊體中心放在那格頂面，
                // 等於腳底（中心再往下 CapsuleHalfHeight）落在地表下方
                // CapsuleHalfHeight - TileSizeCm（Grain=16 預設下約 93.75cm）的地方，整個人
                // 嵌進地裡。加上這個垂直偏移，讓膠囊體底部（腳）剛好對齊到 PlacementTile
                // 那格的下緣＝地表頂面，真正站在地表上而不是嵌入地下。
                const float FeetAlignOffset = WorldScale::CapsuleHalfHeight - WorldScale::TileSizeCm;
                Char->SetActorLocation(WorldScale::TileToWorld(
                    FGridPos(PlacementTile.X, PlacementTile.Y, PlacementTile.Z), VW->WorldHeight)
                    + FVector(0.f, 0.f, FeetAlignOffset));

            }

            Char->OnCharacterDied.AddDynamic(this, &ASkillCreatorGameMode::PerformSave);
            Char->OnCharacterDied.AddDynamic(this, &ASkillCreatorGameMode::OnPlayerDied);
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

    // 記錄這次存檔的 TilePosition 屬於哪個世界（見 CharacterSaveData.h LastWorldId 註解），
    // StartGameplayWithWorld() 要靠這個判斷能不能信任 TilePosition。
    CurrentCharacterSave.LastWorldId = CurrentWorldSave.Id;

    // 把此次遊戲的累積時間寫回世界存檔
    if (UGameClockSubsystem* Clock =
        GetGameInstance()->GetSubsystem<UGameClockSubsystem>())
        CurrentWorldSave.ElapsedTicks = Clock->TotalTicks;

    FFlowSaveSystem::SaveCharacter(CurrentCharacterSave);

    if (AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(GetWorld()))
        FFlowSaveSystem::SaveAll(*VW->GetTileWorld(), CurrentWorldSave);
}

void ASkillCreatorGameMode::OnPlayerDied()
{
    // D-2/D-4: 凍結遊戲 + 釋放滑鼠 + 顯示死亡遮罩（對應 Godot Main.cs:971-979）
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    PC->SetPause(true);
    FInputModeUIOnly UiMode;
    UiMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(UiMode);
    PC->SetShowMouseCursor(true);

    if (ASkillCreatorHUD* HUD = Cast<ASkillCreatorHUD>(PC->GetHUD()))
    if (HUD->HUDWidget)
    {
        HUD->HUDWidget->OnRespawnRequested.BindUObject(
            this, &ASkillCreatorGameMode::OnRespawnRequested);
        HUD->HUDWidget->ShowDeathScreen(true);
    }
}

void ASkillCreatorGameMode::OnRespawnRequested()
{
    // D-3: 復活玩家（對應 Godot Main.cs:1832-1837）
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    if (ASkillCreatorCharacter* Char = Cast<ASkillCreatorCharacter>(PC->GetPawn()))
    {
        // 回滿 HP / MP / 各法力插槽
        Char->CurrentHp = Char->Stats.MaxHpBase;
        Char->CurrentMp = Char->Stats.MaxMpBase;
        for (FManaSlot& Slot : Char->ActiveManaSlots)
            Slot.Current = Slot.Max;

        // 傳送到出生點
        if (AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(GetWorld()))
        {
            const FIntVector ST = CurrentWorldSave.PlayerSpawn;
            const float FeetAlignOffset = WorldScale::CapsuleHalfHeight - WorldScale::TileSizeCm;
            Char->SetActorLocation(WorldScale::TileToWorld(
                FGridPos(ST.X, ST.Y, ST.Z), VW->WorldHeight)
                + FVector(0.f, 0.f, FeetAlignOffset));
        }
    }

    // 解凍遊戲
    PC->SetPause(false);
    PC->SetInputMode(FInputModeGameOnly());
    PC->SetShowMouseCursor(false);

    // 關閉死亡遮罩
    if (ASkillCreatorHUD* HUD = Cast<ASkillCreatorHUD>(PC->GetHUD()))
    if (HUD->HUDWidget)
        HUD->HUDWidget->ShowDeathScreen(false);
}

void ASkillCreatorGameMode::SpawnWorldAndMobs(int32 WorldSeed, const FString& WorldId)
{
    // 若 MainMap 裡沒有手動放置的 AVoxelWorldActor，自動生成一個（用選定的 Seed）
    AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(GetWorld());
    UE_LOG(LogTemp, Log, TEXT("[GameMode] SpawnWorldAndMobs: Seed=%d Id=%s VW_exists=%d"), WorldSeed, *WorldId, VW ? 1 : 0);
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
            // BeginPlay 不再自動呼叫 InitializeWorldState（避免 RMC 雙重初始化），
            // 動態生成的 VW 需在此補一次首次初始化。
            VW->ReinitializeForWorld(WorldSeed, WorldId);
        }
        UE_LOG(LogTemp, Log, TEXT("[GameMode] SpawnWorldAndMobs: fresh VW spawned"));
    }
    // 2026-06-23 修復：上面的 if(!VW) 只處理「這個遊戲進程第一次進世界」——AVoxelWorldActor
    // 是動態 Spawn 出來的單一實例，BeginPlay() 只在第一次 Spawn 時跑過一次。玩家在同一次
    // 執行（沒重開 exe）回到選單再建立第二個世界時，這裡會找到「既存」的 VW（第一個世界
    // 留下的），原本完全沒有任何程式碼更新它的 WorldSeed/WorldSaveDir——導致玩家之後創建
    // 的每個「新世界」其實都還在用第一個世界的 seed + 存檔目錄跑（這就是「Saved/Worlds
    // 永遠都同一個」回報的根因），地表/chunk 全部對不上玩家剛建立的那個新世界的 meta 資料。
    else if (VW->WorldSeed != WorldSeed || VW->WorldSaveDir != WorldId)
    {
        UE_LOG(LogTemp, Log, TEXT("[GameMode] SpawnWorldAndMobs: ReinitializeForWorld (Seed=%d→%d  Id=%s→%s)"),
            VW->WorldSeed, WorldSeed, *VW->WorldSaveDir, *WorldId);
        VW->ReinitializeForWorld(WorldSeed, WorldId);

        // 同一批清理：AEnemyManager/UDroppedItemManager 跟 AVoxelWorldActor 一樣是
        // 「同進程內持續存活、不隨換世界重建」的物件（前者是手動 Spawn 的單一 Actor，
        // 後者是 UWorldSubsystem——本來預期隨關卡銷毀重建，但 MainMap 整個遊戲進程內
        // 從未真正重新載入過，所以這個假設不成立）。換世界時舊世界留下的敵人/掉落物
        // 沒清掉，會帶著舊世界的座標繼續留在新世界裡，對新世界的地形毫無意義。
        for (TActorIterator<AEnemyManager> EmIt(GetWorld()); EmIt; ++EmIt)
        { EmIt->ClearAllEnemies(); break; }
        if (UDroppedItemManager* DropMgr = GetWorld()->GetSubsystem<UDroppedItemManager>())
            DropMgr->Clear();
        if (UCraftingStationSubsystem* CraftSub = GetWorld()->GetSubsystem<UCraftingStationSubsystem>())
            CraftSub->DestroyAllFixtures();
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[GameMode] SpawnWorldAndMobs: VW already correct (Seed=%d Id=%s), no reinit"), WorldSeed, *WorldId);
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
