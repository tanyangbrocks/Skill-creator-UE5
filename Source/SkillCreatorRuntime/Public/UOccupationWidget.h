#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UOccupationWidget.generated.h"

// 職業能力面板（玩家個人面板 Tab 2）— 目前為空白佔位
UCLASS()
class SKILLCREATORRUNTIME_API UOccupationWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeOnInitialized() override;
};
