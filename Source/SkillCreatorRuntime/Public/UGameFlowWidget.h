#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterSaveData.h"
#include "WorldSaveData.h"
#include "UGameFlowWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UButton;
class UEditableText;
class UScrollBox;
class UProgressBar;
class FTileWorld3D;
class FMapGenerator3D;
class UPlayerCreateWidget;

USTRUCT(BlueprintType)
struct FWorldSaveInfo
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Id;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) int32   Seed    = 12345;
    UPROPERTY(BlueprintReadOnly) FString WorldDir;
};

// 角色清單顯示用的輕量摘要（對應 Godot 角色卡片顯示的 Name + Level）。
// 完整存檔資料（Hp/Mp/Inventory/...）見 FCharacterSaveData，選定角色後才整份載入。
USTRUCT(BlueprintType)
struct FCharacterSummaryInfo
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Id;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) int32   Level = 1;
};

// 角色選擇/管理 + 世界選擇/管理 Widget。不需要 Blueprint 子類或 WBP .uasset，
// 直接 CreateWidget<UGameFlowWidget>(PC, UGameFlowWidget::StaticClass())。
// 如需客製化外觀，建立 Blueprint 子類並覆寫 BlueprintNativeEvent。
//
// 完整對齊 Godot GameFlowUI.cs 的 5 畫面流程（2026-06-20 重做，取代先前「角色/世界各一畫面、
// 創建內嵌在同畫面」的精簡版）：
//   標題畫面（SkillCreator + 進入遊戲）
//   → 角色選擇畫面（卡片可點擊選取，🗑 刪除；「創建角色」導向角色創建畫面）
//   → 角色創建畫面（名稱輸入 + 確認創建 + 返回角色選擇）
//   → 世界選擇畫面（卡片可點擊進入，🗑 刪除；「創建世界」導向世界創建畫面；返回角色選擇）
//   → 世界創建畫面（名稱輸入 + 確認創建（含 loading 遮罩 + 立即預生成出生區地形）+ 返回世界選擇）
//   → 進入遊戲
// 清單卡片用 UFlowCardWidget（對應 Godot MakeCharCard/MakeWorldCard 的整塊可點擊卡片 + 獨立刪除鈕），
// 不再用「文字輸入 ID」這種暫代介面。
UCLASS()
class SKILLCREATORRUNTIME_API UGameFlowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // 解構函式定義在 .cpp（那裡才有 FTileWorld3D/FMapGenerator3D 完整型別），手動
    // delete PendingGenWorld/PendingGenGen（兩者改用 raw pointer，不是 TUniquePtr——
    // UHT 除了一般建構/解構函式，還會生成一個 VTable helper 建構子
    // DEFINE_VTABLE_PTR_HELPER_CTOR_NS，這個一樣需要完整型別才能具現化 TUniquePtr 的
    // 刪除器，光宣告 explicit 建構/解構函式擋不住它；raw pointer 本身是 trivially
    // destructible，不管指向的型別有沒有完整定義都不影響，從根上避開整個問題）。
    virtual ~UGameFlowWidget() override;

    // ── 世界 ──────────────────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="GameFlow") void RefreshWorldList();
    UFUNCTION(BlueprintCallable, Category="GameFlow") bool CreateWorld(const FString& Name, int32 Seed);
    UFUNCTION(BlueprintCallable, Category="GameFlow") bool RemoveWorld(const FString& WorldId);
    UFUNCTION(BlueprintCallable, Category="GameFlow") void EnterWorld(const FString& WorldId);
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="GameFlow")
    const TArray<FWorldSaveInfo>& GetCachedWorldList() const { return CachedWorlds; }

    // ── 角色 ──────────────────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="GameFlow") void RefreshCharacterList();
    UFUNCTION(BlueprintCallable, Category="GameFlow") bool CreateCharacter(const FString& Name);
    UFUNCTION(BlueprintCallable, Category="GameFlow") bool RemoveCharacter(const FString& CharacterId);
    // 選定角色：整份 FCharacterSaveData 載入快取，並切換到世界畫面
    UFUNCTION(BlueprintCallable, Category="GameFlow") void SelectCharacter(const FString& CharacterId);
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="GameFlow")
    const TArray<FCharacterSummaryInfo>& GetCachedCharacterList() const { return CachedCharacters; }

    // C++ 有預設實作；Blueprint 子類可覆寫以客製化外觀
    UFUNCTION(BlueprintNativeEvent, Category="GameFlow")
    void OnWorldListRefreshed(const TArray<FWorldSaveInfo>& Worlds);
    virtual void OnWorldListRefreshed_Implementation(const TArray<FWorldSaveInfo>& Worlds);

    UFUNCTION(BlueprintNativeEvent, Category="GameFlow")
    void OnCharacterListRefreshed(const TArray<FCharacterSummaryInfo>& Characters);
    virtual void OnCharacterListRefreshed_Implementation(const TArray<FCharacterSummaryInfo>& Characters);

    UFUNCTION(BlueprintNativeEvent, Category="GameFlow")
    void OnEnterGame(const FWorldSaveInfo& SelectedWorld);
    virtual void OnEnterGame_Implementation(const FWorldSaveInfo& SelectedWorld);

protected:
    // 故意覆寫 NativeOnInitialized()，不是 NativeConstruct()：
    // UWidget::TakeWidget_Private() 第一次呼叫時，會先呼叫 RebuildWidget()（讀取
    // WidgetTree->RootWidget，null 就回退成空的 SNew(SSpacer)），然後才呼叫
    // OnWidgetRebuilt() → NativeConstruct()。也就是說在 NativeConstruct() 裡才設定
    // WidgetTree->RootWidget 永遠太晚——第一次 AddToViewport() 一定拿到空畫面，
    // 之後 WidgetTree 怎麼變都不會再重新 take（MyWidget 已經被快取）。
    // NativeOnInitialized() 在 UUserWidget::Initialize() 裡呼叫，Initialize() 是
    // CreateWidget() 當下就執行（早於 AddToViewport()），所以這裡設定 RootWidget
    // 才能在第一次 RebuildWidget() 讀到它之前生效。
    virtual void NativeOnInitialized() override;

    // 2026-06-22：世界生成輪詢改用 NativeTick() 而非 FTimerManager::SetTimer()，原因見
    // PendingGenWorld 等成員上方的註解——FTimerManager::Tick() 在 UWorld::Tick() 裡被
    // `if (TickType != LEVELTICK_TimeOnly && !bIsPaused)` 包住（Engine/Private/LevelTick.cpp
    // 行 1783-1788），GameFlowWidget 顯示主選單時遊戲是暫停的，所以原本的 Timer 永遠不會
    // 觸發，世界生成永遠卡在第一個畫面（使用者實測回報「卡在世界生成，進不去」）。
    // UUserWidget::NativeTick() 走 Slate 自己的 tick 迴圈，沒有任何暫停閘門（已確認
    // UMG UserWidget.cpp 整支沒有 bIsPaused 相關程式碼），暫停選單期間一樣會被呼叫。
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    // ── 畫面切換（對應 Godot ShowScreen，GameFlowUI.cs:90-97）────────────────
    UPROPERTY() TObjectPtr<UWidget> TitleScreen        = nullptr;
    UPROPERTY() TObjectPtr<UWidget> CharSelectScreen   = nullptr;
    UPROPERTY() TObjectPtr<UWidget> WorldSelectScreen  = nullptr;
    UPROPERTY() TObjectPtr<UWidget> WorldCreateScreen  = nullptr;
    // W-10：角色創建改用獨立的 11 步精靈（UPlayerCreateWidget），取代原本「輸入名字+確認」
    // 一頁式 CharCreateScreen（見 docs/plan-w10-character-creation.md）。Lazy 建立（第一次點
    // 「創建角色」才 CreateWidget），加進跟其他畫面同一個 ScreenStack Overlay，沿用同一套
    // ShowScreen() 機制。
    UPROPERTY() TObjectPtr<UPlayerCreateWidget> PlayerCreateScreen = nullptr;
    void ShowScreen(UWidget* Target);

    // 角色選擇畫面
    UPROPERTY() TObjectPtr<UVerticalBox> CharListContainer = nullptr;

    // 世界選擇畫面
    UPROPERTY() TObjectPtr<UVerticalBox> WorldListContainer = nullptr;
    // 世界創建畫面
    UPROPERTY() TObjectPtr<UEditableText> WorldNameInput = nullptr;

    // 世界生成 loading 遮罩（對應 Godot _genLoadingOverlay/_genLoadingLabel，GameFlowUI.cs:399-480）
    UPROPERTY() TObjectPtr<UWidget>       WorldGenLoadingOverlay = nullptr;
    UPROPERTY() TObjectPtr<UTextBlock>    WorldGenLoadingLabel   = nullptr;
    UPROPERTY() TObjectPtr<UProgressBar>  WorldGenProgressBar    = nullptr;

    // 世界生成非同步狀態：原本用同步 busy-wait（Sleep+迴圈）等所有 chunk 算完，會把整個
    // 視窗的訊息迴圈一起凍住到 OS 認為「沒有回應」（2026-06-21 使用者實測回報「卡住了」，
    // 詳見 docs/開發血汗錄.md）。改成逐 tick 輪詢 FMapGenerator3D 的非同步佇列
    // （EnsureChunksAround 本身就是丟到 UE::Tasks 背景執行緒，ApplyPendingChunks 只是把
    // 已算完的結果搬進 World，本來就是安全、快速、可逐 tick 呼叫的)，順便能畫進度文字/條。
    // 2026-06-22：輪詢驅動改用 NativeTick()（見上方宣告旁註解），不再用 FTimerManager::
    // SetTimer()——選單暫停期間 Timer 永遠不會觸發，會讓世界生成卡死。
    FTileWorld3D*    PendingGenWorld = nullptr;
    FMapGenerator3D* PendingGenGen   = nullptr;
    FWorldSaveData             PendingGenMeta;
    int32                      PendingGenTotalChunks = 0;
    bool                       bIsGeneratingWorld = false;

    void StartWorldGeneration(const FString& Name, int32 Seed);
    void PollWorldGeneration();
    void FinishWorldGeneration();

    UPROPERTY() TArray<FWorldSaveInfo>        CachedWorlds;
    UPROPERTY() TArray<FCharacterSummaryInfo> CachedCharacters;
    // 玩家選定角色的完整存檔（不只摘要），進遊戲時交給 GameMode 套用到 Pawn
    UPROPERTY() FCharacterSaveData SelectedCharacter;

    void BuildLayout();
    UWidget* BuildTitleScreen();
    UWidget* BuildCharSelectScreen();
    UWidget* BuildWorldSelectScreen();
    UWidget* BuildWorldCreateScreen();

    UFUNCTION() void OnTitleEnterClicked();

    UFUNCTION() void OnCharCreateNavClicked();   // 角色選擇 →「創建角色」按鈕：開 PlayerCreateScreen 精靈

    UFUNCTION() void OnWorldSelectBackClicked();  // 世界選擇 →「← 返回角色選擇」
    UFUNCTION() void OnWorldCreateNavClicked();   // 世界選擇 →「創建世界」按鈕
    UFUNCTION() void OnWorldCreateBackClicked();  // 世界創建 →「← 返回」
    UFUNCTION() void OnWorldCreateConfirmClicked();

    // ── 確認刪除彈窗（對應 Godot ShowConfirm，GameFlowUI.cs:146-151）──────────
    UPROPERTY() TObjectPtr<UWidget>    ConfirmOverlay  = nullptr;
    UPROPERTY() TObjectPtr<UTextBlock> ConfirmMsgText  = nullptr;
    FString PendingDeleteId;
    bool    bPendingDeleteIsChar = false;

    void ShowConfirmDelete(const FString& Id, const FString& Name, bool bIsChar);

    UFUNCTION() void OnConfirmDeleteYes();
    UFUNCTION() void OnConfirmDeleteNo();
};
