#pragma once
#include "CoreMinimal.h"
#include "ICollectible.generated.h"

// W-D：可採集 Entity 介面（玩家左鍵固定 0.5s 後呼叫 Collect()）
// 實作者：AWeedEntity（W-G），以後擴充 AOakSaplingEntity 等
UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UCollectible : public UInterface
{
    GENERATED_BODY()
};

class SKILLCREATORRUNTIME_API ICollectible
{
    GENERATED_BODY()
public:
    // 採集完成時呼叫（負責掉落物品、自毀 Actor）
    virtual void Collect(APlayerController* Collector) = 0;

    // 給 HUD / 準心顯示用：是否可被當前玩家採集（距離/條件檢查）
    virtual bool CanBeCollected(const APlayerController* Collector) const { return true; }
};
