#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IInteractable.generated.h"

class ASkillCreatorCharacter;

// 右鍵互動介面（docs/plan-item-crafting-system.md §五）。寶箱/工作臺等不可塑形可放置物
// 的 Actor 實作此介面；用 UINTERFACE+雙生宏（不是純虛擬基類）是因為要 Cast<> 到泛型
// AActor 上、給 Blueprint 子類也能覆寫（依 CLAUDE.md「給 Blueprint/AActor 用的對外接口」慣例）。
UINTERFACE(BlueprintType)
class SKILLCREATORRUNTIME_API UInteractable : public UInterface
{
    GENERATED_BODY()
};

class SKILLCREATORRUNTIME_API IInteractable
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, Category="Interact")
    void Interact(ASkillCreatorCharacter* Player);

    // 之後可選：互動提示文字（例如 HUD 顯示「按右鍵開啟」），先給空字串
    UFUNCTION(BlueprintNativeEvent, Category="Interact")
    FText GetInteractPrompt() const;
};
