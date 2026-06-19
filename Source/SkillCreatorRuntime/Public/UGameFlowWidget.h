#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterSaveData.h"
#include "UGameFlowWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UButton;
class UEditableText;
class UScrollBox;

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
// 流程（對應 Godot GameFlowUI：標題 → 角色選擇/創建 → 世界選擇/創建 → 進入遊戲）：
//   角色畫面（選擇/建立角色，獨立於任何世界）→ 選定角色後 → 世界畫面（選擇/建立世界）→ 進入遊戲。
// 角色與世界的「建立」都用同一畫面內的輸入框 + 按鈕（沿用世界畫面既有的單畫面內嵌建立模式，
// 不額外拆成 Godot 那種獨立的創建子畫面），兩個流程只用一個「← 返回」在世界畫面切回角色畫面。
UCLASS()
class SKILLCREATORRUNTIME_API UGameFlowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
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
    virtual void NativeConstruct() override;

private:
    // ── 畫面切換（對應 Godot ShowScreen）──────────────────────────────────
    UPROPERTY() TObjectPtr<UWidget> CharScreen  = nullptr;
    UPROPERTY() TObjectPtr<UWidget> WorldScreen = nullptr;
    void ShowScreen(UWidget* Target);

    // 動態建立的子 widget 參照（角色畫面）
    UPROPERTY() TObjectPtr<UVerticalBox>  CharListContainer = nullptr;
    UPROPERTY() TObjectPtr<UEditableText> CharNameInput     = nullptr;
    UPROPERTY() TObjectPtr<UEditableText> CharIdInput       = nullptr;

    // 動態建立的子 widget 參照（世界畫面）
    UPROPERTY() TObjectPtr<UVerticalBox>  WorldListContainer = nullptr;
    UPROPERTY() TObjectPtr<UEditableText> WorldNameInput     = nullptr;
    UPROPERTY() TObjectPtr<UEditableText> WorldIdInput       = nullptr;

    UPROPERTY() TObjectPtr<UTextBlock> StatusText = nullptr;

    UPROPERTY() TArray<FWorldSaveInfo>        CachedWorlds;
    UPROPERTY() TArray<FCharacterSummaryInfo> CachedCharacters;
    // 玩家選定角色的完整存檔（不只摘要），進遊戲時交給 GameMode 套用到 Pawn
    UPROPERTY() FCharacterSaveData SelectedCharacter;

    void BuildLayout();
    UWidget* BuildCharScreen();
    UWidget* BuildWorldScreen();

    UFUNCTION() void OnCharCreateClicked();
    UFUNCTION() void OnCharSelectClicked();
    UFUNCTION() void OnCharDeleteClicked();
    UFUNCTION() void OnCharRefreshClicked();

    UFUNCTION() void OnWorldCreateClicked();
    UFUNCTION() void OnWorldEnterClicked();
    UFUNCTION() void OnWorldDeleteClicked();
    UFUNCTION() void OnWorldRefreshClicked();
    UFUNCTION() void OnWorldBackClicked();

    // ── 確認刪除彈窗 ────────────────────────────────────────────────
    UPROPERTY() TObjectPtr<UWidget>    ConfirmOverlay  = nullptr;
    UPROPERTY() TObjectPtr<UTextBlock> ConfirmMsgText  = nullptr;
    FString PendingDeleteId;
    bool    bPendingDeleteIsChar = false;

    void ShowConfirmDelete(const FString& Id, bool bIsChar);

    UFUNCTION() void OnConfirmDeleteYes();
    UFUNCTION() void OnConfirmDeleteNo();
};
