#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UCompendiumWidget.generated.h"

// 圖鑑全版介面（右上角「圖鑑」按鈕觸發）— 目前為空白佔位
UCLASS()
class SKILLCREATORRUNTIME_API UCompendiumWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeOnInitialized() override;
};
