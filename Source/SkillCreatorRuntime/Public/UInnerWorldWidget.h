#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UInnerWorldWidget.generated.h"

// 內部空間面板（玩家個人面板 Tab 4）— 目前為空白佔位
UCLASS()
class SKILLCREATORRUNTIME_API UInnerWorldWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeOnInitialized() override;
};
