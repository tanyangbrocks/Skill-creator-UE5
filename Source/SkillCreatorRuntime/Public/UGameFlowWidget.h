#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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

// 世界選擇/管理 Widget。不需要 Blueprint 子類或 WBP .uasset，
// 直接 CreateWidget<UGameFlowWidget>(PC, UGameFlowWidget::StaticClass())。
// 如需客製化外觀，建立 Blueprint 子類並覆寫 BlueprintNativeEvent。
UCLASS()
class SKILLCREATORRUNTIME_API UGameFlowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="GameFlow") void RefreshWorldList();
    UFUNCTION(BlueprintCallable, Category="GameFlow") bool CreateWorld(const FString& Name, int32 Seed);
    UFUNCTION(BlueprintCallable, Category="GameFlow") bool RemoveWorld(const FString& WorldId);
    UFUNCTION(BlueprintCallable, Category="GameFlow") void EnterWorld(const FString& WorldId);
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="GameFlow")
    const TArray<FWorldSaveInfo>& GetCachedWorldList() const { return CachedWorlds; }

    // C++ 有預設實作；Blueprint 子類可覆寫以客製化外觀
    UFUNCTION(BlueprintNativeEvent, Category="GameFlow")
    void OnWorldListRefreshed(const TArray<FWorldSaveInfo>& Worlds);
    virtual void OnWorldListRefreshed_Implementation(const TArray<FWorldSaveInfo>& Worlds);

    UFUNCTION(BlueprintNativeEvent, Category="GameFlow")
    void OnEnterGame(const FWorldSaveInfo& SelectedWorld);
    virtual void OnEnterGame_Implementation(const FWorldSaveInfo& SelectedWorld);

protected:
    virtual void NativeConstruct() override;

private:
    // 動態建立的子 widget 參照
    UPROPERTY() TObjectPtr<UVerticalBox>  WorldListContainer = nullptr;
    UPROPERTY() TObjectPtr<UEditableText> NameInput          = nullptr;
    UPROPERTY() TObjectPtr<UEditableText> WorldIdInput       = nullptr;
    UPROPERTY() TObjectPtr<UTextBlock>    StatusText         = nullptr;

    UPROPERTY() TArray<FWorldSaveInfo> CachedWorlds;

    void BuildLayout();

    UFUNCTION() void OnCreateClicked();
    UFUNCTION() void OnEnterClicked();
    UFUNCTION() void OnDeleteClicked();
    UFUNCTION() void OnRefreshClicked();
};
