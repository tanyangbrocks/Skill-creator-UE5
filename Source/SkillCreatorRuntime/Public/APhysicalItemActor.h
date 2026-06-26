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

    // 手動物理狀態
    FVector Velocity  = FVector::ZeroVector;  // cm/s
    bool    bLanded   = false;

    void PhysicsTick(float DeltaTime);
    void HandleTileCollision(int32 VoxX, int32 VoxY, int32 VoxZ);
    bool IsSolidAt(int32 VoxX, int32 VoxY, int32 VoxZ) const;
    float GetBelowRestitution() const;
};
