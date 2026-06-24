#pragma once
#include "CoreMinimal.h"
#include "AVoxelizableActor.h"
#include "VoxelAsset.h"
#include "WorldTypes.h"
#include "Engine/StreamableManager.h"
#include "ADestructibleMeshActor.generated.h"

// 路線 B：平時 StaticMesh 展示，毀壞時注入體素並走碎片管線
// （plan-voxelization.md §十）
UCLASS()
class SKILLCREATORRUNTIME_API ADestructibleMeshActor : public AVoxelizableActor
{
    GENERATED_BODY()
public:
    ADestructibleMeshActor();

    // 平時渲染的 StaticMesh
    UPROPERTY(VisibleAnywhere, Category="Voxelization")
    UStaticMeshComponent* DisplayMesh = nullptr;

    // 直接指派 VoxelAsset（測試用；正式流程用 VoxelAssetId 查 DataTable）
    UPROPERTY(EditAnywhere, Category="Voxelization")
    UVoxelAsset* VoxelAssetDirect = nullptr;

    // DataTable row 名稱（正式流程：從 DA_VoxelAssetRegistry 查詢）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxelization")
    FName VoxelAssetId;

    // 觸發毀壞：隱藏 DisplayMesh → 注入體素 → 呼叫 TriggerVoxelDestruction → Destroy()
    UFUNCTION(BlueprintCallable, Category="Voxelization")
    void TriggerDestruction(EDestroyReason Reason, float Intensity = 1.f,
                            FVector SlashDirection = FVector::ZeroVector);

protected:
    virtual void BeginPlay() override;

    // AVoxelizableActor 介面（Auto 模式未使用，但必須實作純虛函式）
    virtual void SwitchToFullVoxel()   override {}
    virtual void SwitchToMeshDisplay() override { if (DisplayMesh) DisplayMesh->SetVisibility(true); }

private:
    UVoxelAsset* ResolveAsset() const;
};
