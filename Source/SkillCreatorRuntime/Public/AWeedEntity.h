#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ICollectible.h"
#include "AWeedEntity.generated.h"

// W-G：草地雜草 Entity
// - 由 AVoxelWorldActor 的 random tick 在 Grass tile + Air 條件下生成
// - 玩家左鍵 0.5s 採集（ICollectible），掉落 EItemId::Weed
// - 採集後自毀；若下方 tile 變成非 Grass，也自毀（每幀檢查）
UCLASS()
class SKILLCREATORRUNTIME_API AWeedEntity : public AActor, public ICollectible
{
    GENERATED_BODY()
public:
    AWeedEntity();

    // ICollectible
    virtual void Collect(APlayerController* Collector) override;
    virtual bool CanBeCollected(const APlayerController* Collector) const override { return true; }

    // 綁定到哪個 Grass tile（tile 座標，UE5 tile space）
    FIntVector TilePos = FIntVector::ZeroValue;

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

private:
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> MeshComp;

    // 快取，避免每幀查 Subsystem
    TWeakObjectPtr<class AVoxelWorldActor> CachedVoxelWorld;
};
