#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "USpellGroupWidget.generated.h"

class UButton;
class UTextBlock;

// 技能組切換面板（V 鍵開關）：5 個技能組選擇按鈕
UCLASS()
class SKILLCREATORRUNTIME_API USpellGroupWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    static constexpr int32 MaxGroups = 5;
    int32 ActiveGroupIndex = 0;

    TDelegate<void(int32)> OnGroupSelected;
    TDelegate<void()>      OnCloseRequested;

    void SetActiveGroup(int32 Idx);

protected:
    virtual void NativeConstruct() override;

private:
    TObjectPtr<UButton>    GroupButtons[MaxGroups];
    TObjectPtr<UTextBlock> GroupButtonTexts[MaxGroups];

    UFUNCTION() void OnGroup0Clicked();
    UFUNCTION() void OnGroup1Clicked();
    UFUNCTION() void OnGroup2Clicked();
    UFUNCTION() void OnGroup3Clicked();
    UFUNCTION() void OnGroup4Clicked();

    void SelectGroup(int32 Idx);
    void RefreshButtonHighlight();
};
