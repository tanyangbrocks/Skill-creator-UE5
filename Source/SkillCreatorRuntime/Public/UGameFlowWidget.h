#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UGameFlowWidget.generated.h"

// Blueprint 可見的輕量世界描述（從 FWorldSaveData 轉換，避免 VoxelWorld 型別洩漏）
USTRUCT(BlueprintType)
struct FWorldSaveInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString Id;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) int32   Seed    = 12345;
    UPROPERTY(BlueprintReadOnly) FString WorldDir;
};

// GameFlow 選單 Widget C++ 基底（對應 Godot GameFlowUI.cs）。
// 繼承此類建立 WBP_GameFlow Blueprint，在 NativeConstruct 後
// OnWorldListRefreshed 會自動被呼叫，Blueprint 負責渲染清單。
// 玩家點擊進入 → Blueprint 呼叫 EnterWorld(Id) → OnEnterGame 觸發 → Blueprint OpenLevel
UCLASS(Abstract)
class SKILLCREATORRUNTIME_API UGameFlowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // ── Blueprint Callable ────────────────────────────────────────────────
    // 重新掃描存檔資料夾並廣播 OnWorldListRefreshed
    UFUNCTION(BlueprintCallable, Category="GameFlow")
    void RefreshWorldList();

    // 建立新世界並重整清單；失敗返回 false
    UFUNCTION(BlueprintCallable, Category="GameFlow")
    bool CreateWorld(const FString& Name, int32 Seed);

    // 刪除世界（不可逆）並重整清單；失敗返回 false
    UFUNCTION(BlueprintCallable, Category="GameFlow")
    bool RemoveWorld(const FString& WorldId);

    // 選定世界 → 存入 GameSessionSubsystem → 廣播 OnEnterGame
    UFUNCTION(BlueprintCallable, Category="GameFlow")
    void EnterWorld(const FString& WorldId);

    // 取得最新一次 Refresh 的快取清單（Blueprint 讀取用）
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="GameFlow")
    const TArray<FWorldSaveInfo>& GetCachedWorldList() const { return CachedWorlds; }

    // ── Blueprint Implementable Events ────────────────────────────────────
    // 每次 RefreshWorldList() 完成後呼叫
    UFUNCTION(BlueprintImplementableEvent, Category="GameFlow")
    void OnWorldListRefreshed(const TArray<FWorldSaveInfo>& Worlds);

    // 玩家確認進入某世界後呼叫；Blueprint 在此執行 OpenLevel
    UFUNCTION(BlueprintImplementableEvent, Category="GameFlow")
    void OnEnterGame(const FWorldSaveInfo& SelectedWorld);

protected:
    virtual void NativeConstruct() override;

private:
    UPROPERTY() TArray<FWorldSaveInfo> CachedWorlds;
};
