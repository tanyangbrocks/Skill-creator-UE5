#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemId.h"
#include "ADebrisActor.generated.h"

// 物理碎塊實體（docs/plan-debris-fragment.md §D-1）
// 爆炸/斬擊/碾壓產生；UE5 Chaos 剛體物理（SimulatePhysics=true）。
// 撿起後 FragmentCount 個 FragmentItemId 加入物品欄；由 UDroppedItemManager 追蹤。
// Phase 2：Mesh 換成 Chaos Fracture 幾何碎片，其餘介面不變。
UCLASS()
class SKILLCREATORRUNTIME_API ADebrisActor : public AActor
{
    GENERATED_BODY()
public:
    ADebrisActor();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Debris")
    EItemId FragmentItemId = EItemId::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Debris")
    int32 FragmentCount = 0;

    // 撿取半徑（cm）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris")
    float PickupRadius = 80.f;

    // 存活時間（秒；0=不消失）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris")
    float Lifetime = 30.f;

    void Init(EItemId InId, int32 InCount);

    // 給予初始速度（cm/s，bVelChange=true 略過質量直接設速度）
    void LaunchImpulse(FVector VelocityCms);

    bool CanPickup(FVector PlayerWorldPos) const;

    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* Mesh = nullptr;

    float Age = 0.f;
};
