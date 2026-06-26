#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemId.h"
#include "IPhysicalPickable.h"
#include "ADebrisActor.generated.h"

class USphereComponent;
class ASkillCreatorCharacter;

// 物理碎塊實體（docs/plan-debris-fragment.md §D-1）
// 爆炸/斬擊/碾壓產生；UE5 Chaos 剛體物理（SimulatePhysics=true）。
// G-10：實作 IPhysicalPickable，讓碎塊可被 G 鍵撿取並轉換為物品欄碎片物品。
// Phase 2：Mesh 換成 Chaos Fracture 幾何碎片，其餘介面不變。
UCLASS()
class SKILLCREATORRUNTIME_API ADebrisActor : public AActor, public IPhysicalPickable
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

    // IPhysicalPickable
    float   GetMass()            const override { return 0.5f; }  // 碎塊固定質量（輕）
    EItemId GetInventoryItemId() const override { return FragmentItemId; }
    int32   GetInventoryCount()  const override { return FragmentCount; }
    FText   GetPickupHintText()  const override;
    void    OnCarried(ASkillCreatorCharacter* Carrier) override;
    void    OnReleased(FVector ThrowVelocityCms) override;
    bool    IsPickable()         const override { return !bBeingCarried; }

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USphereComponent> PickupSphere;

    float Age = 0.f;
    bool  bBeingCarried = false;
    TWeakObjectPtr<ASkillCreatorCharacter> Carrier;
};
