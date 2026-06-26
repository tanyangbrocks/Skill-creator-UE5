#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UCraftingHintCardWidget.generated.h"

class UTextBlock;

// 加工選單提示圖卡：常駐顯示在畫面左側，右下角顯示「可加工物總數」計數器。
// 不需要滑鼠互動。Shift 展開完整加工面板（UCraftingPanelWidget）。
UCLASS()
class SKILLCREATORRUNTIME_API UCraftingHintCardWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void UpdateCount(int32 Count);

protected:
    virtual void NativeOnInitialized() override;

private:
    TObjectPtr<UTextBlock> CountLabel = nullptr;
};
