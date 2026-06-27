#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IPhysicalPickable.h"
#include "ItemId.h"
#include "APhysicalItemActor.generated.h"

class AVoxelWorldActor;
class ASkillCreatorCharacter;
class USphereComponent;

UCLASS()
class SKILLCREATORRUNTIME_API APhysicalItemActor : public AActor, public IPhysicalPickable
{
    GENERATED_BODY()
public:
    APhysicalItemActor();

    // 初始化（由投擲邏輯或 G-9 呼叫）
    void Init(EItemId InItemId, int32 InCount, FVector InitialVelocityCms = FVector::ZeroVector);

    // IPhysicalPickable
    float   GetMass()            const override;
    EItemId GetInventoryItemId() const override { return ItemId; }
    int32   GetInventoryCount()  const override { return Count; }
    FText   GetPickupHintText()  const override;
    void    OnCarried(ASkillCreatorCharacter* Carrier) override;
    void    OnReleased(FVector ThrowVelocityCms) override;
    bool    IsPickable()         const override { return !bBeingCarried; }

    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> MeshComp;
    UPROPERTY() TObjectPtr<USphereComponent>     PickupSphere;
    UPROPERTY() TWeakObjectPtr<AVoxelWorldActor> CachedVoxelWorld;
    UPROPERTY() TWeakObjectPtr<ASkillCreatorCharacter> Carrier;

    EItemId ItemId  = EItemId::None;
    int32   Count   = 1;
    bool    bBeingCarried = false;

    // PHYS-3: Chaos Physics 取代手動模擬（Velocity/bLanded 已移除）
    // PHYS-4: 液體浮力狀態（Tick 查 tile → SetLinearDamping + AddForce）
    bool bIsInLiquid = false;
};
