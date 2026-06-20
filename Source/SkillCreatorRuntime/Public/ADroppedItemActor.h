#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemId.h"
#include "GridPos.h"
#include "ADroppedItemActor.generated.h"

class AVoxelWorldActor;

// 世界中掉落的物品實體（對應 Godot DroppedItem.cs）。
// 由 UDroppedItemManager::SpawnDrop 建立；玩家進入 PickupRadius 後可拾取。
UCLASS()
class SKILLCREATORRUNTIME_API ADroppedItemActor : public AActor
{
    GENERATED_BODY()
public:
    ADroppedItemActor();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drop")
    EItemId ItemId   = EItemId::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Drop")
    int32   Count    = 1;

    // 拾取半徑（tile 單位）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drop")
    float   PickupRadius = 1.5f;

    // 生命週期（秒；0 = 不消失）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drop")
    float   Lifetime = 60.f;

    // 拾取後自行銷毀並呼叫此 delegate（UDroppedItemManager 注入）
    TFunction<void(ADroppedItemActor*)> OnPickedUp;

    // 初始化，建立後立刻呼叫
    void Init(EItemId InId, int32 InCount);

    // 每幀呼叫；處理 Lifetime 計時和拾取偵測
    virtual void Tick(float DeltaTime) override;

    bool CanPickup(FVector PlayerWorldPos) const;
    bool IsExpired() const { return Lifetime > 0.f && Age >= Lifetime; }

    // 注入世界參照（由 UDroppedItemManager::SpawnDrop 在生成後設定）
    UPROPERTY() TObjectPtr<AVoxelWorldActor> CachedVoxelWorld = nullptr;

private:
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* MeshComp = nullptr;

    float Age      = 0.f;
    float FallTimer = 0.f;  // J-12：重力計時器（對應 Godot DroppedItem.cs:22 FallInterval=0.18f）
};
