#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UFloatingDamageWidget.generated.h"

class UTextBlock;

// 浮動傷害數字 UMG Widget（純 C++ 程式化 layout，不需要 WBP .uasset）
// 由 AFloatingDamageActor 的 UWidgetComponent 驅動
UCLASS()
class SKILLCREATORRUNTIME_API UFloatingDamageWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetDamageText(float Amount, bool bIsCrit);
    void SetDamageColor(FLinearColor Color);

    // Alpha 0-1；由 AFloatingDamageActor 每幀寫入以驅動淡出
    void SetAlpha(float Alpha);

protected:
    virtual void NativeConstruct() override;

private:
    void BuildLayout();

    UPROPERTY() TObjectPtr<UTextBlock> DamageText;
};
