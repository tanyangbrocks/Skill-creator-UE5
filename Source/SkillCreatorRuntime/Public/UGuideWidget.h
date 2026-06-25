#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UGuideWidget.generated.h"

// 引導全版介面（右上角「引導」按鈕觸發）— 目前為空白佔位
UCLASS()
class SKILLCREATORRUNTIME_API UGuideWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeOnInitialized() override;
};
