#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SpecialStatusTypes.h"
#include "UAbnormalStatusBarWidget.generated.h"

class UBorder;
class UTextBlock;
class UHorizontalBox;

// Terraria 風格異常狀態圖示列（頂部中央，最多 15 個圖示）
// 每格 20×20px，正面狀態藍色框，負面狀態橙色框。
UCLASS(BlueprintType, Blueprintable)
class SKILLCREATORRUNTIME_API UAbnormalStatusBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void UpdateStatuses(const TArray<FStatusDisplaySnapshot>& Snaps);

protected:
    virtual void NativeOnInitialized() override;

private:
    static constexpr int32 MaxIcons = 15;
    static constexpr float IconSize = 20.f;

    TObjectPtr<UHorizontalBox>     IconBox;
    TArray<TObjectPtr<UBorder>>    IconBorders;
    TArray<TObjectPtr<UTextBlock>> IconLabels;
    TArray<TObjectPtr<UTextBlock>> StackLabels;
    TArray<TObjectPtr<UTextBlock>> TimerLabels;

    static FLinearColor BorderColor(EAbnormalPolarity Polarity);
    static FLinearColor FillColor  (EAbnormalPolarity Polarity);
};
