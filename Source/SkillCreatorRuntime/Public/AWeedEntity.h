#pragma once
#include "CoreMinimal.h"
#include "AWorldUnit.h"
#include "ICollectible.h"
#include "AWeedEntity.generated.h"

// W-G：草地雜草 Entity
// - 由 ASkillCreatorCharacter::TickWeedGrowth random tick 在 Grass + Air 條件下生成
// - 玩家左鍵 0.5s 採集（ICollectible），掉落 EItemId::Weed
// - 採集後自毀；若 AnchorTile 不再是 Grass，也自毀（Tick 輪詢）
UCLASS()
class SKILLCREATORRUNTIME_API AWeedEntity : public AWorldUnit, public ICollectible
{
    GENERATED_BODY()
public:
    AWeedEntity();

    // ICollectible
    virtual void Collect(APlayerController* Collector) override;
    virtual bool CanBeCollected(const APlayerController* Collector) const override { return true; }

    virtual void Tick(float DeltaTime) override;
};
