#pragma once
#include "CoreMinimal.h"
#include "AVoxelizableActor.h"
#include "VoxelAsset.h"
#include "WorldTypes.h"
#include "ADestructibleMeshActor.generated.h"

// 路線 B：平時 StaticMesh 展示，毀壞時注入體素並走碎片管線
// （plan-voxelization.md §十）
UCLASS()
class SKILLCREATORRUNTIME_API ADestructibleMeshActor : public AVoxelizableActor
{
    GENERATED_BODY()
public:
    ADestructibleMeshActor();

    UPROPERTY(VisibleAnywhere, Category="Voxelization")
    UStaticMeshComponent* DisplayMesh = nullptr;

    // 優先：直接指派 UVoxelAsset（Editor 預建 DataAsset 備用路徑，通常不需要）
    UPROPERTY(EditAnywhere, Category="Voxelization")
    UVoxelAsset* VoxelAssetDirect = nullptr;

    // 主要路徑：相對 Content/ 目錄的 .vox 檔案路徑，例如 "VoxelAssets/rock_small.vox"
    // 打包時需在 DefaultGame.ini [Staging] 加 +AdditionalNonUSFDirectories=Content/VoxelAssets
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxelization")
    FString VoxFilePath;

    UFUNCTION(BlueprintCallable, Category="Voxelization")
    void TriggerDestruction(EDestroyReason Reason, float Intensity = 1.f,
                            FVector SlashDirection = FVector::ZeroVector);

protected:
    virtual void BeginPlay() override;
    virtual void SwitchToFullVoxel()   override {}
    virtual void SwitchToMeshDisplay() override { if (DisplayMesh) DisplayMesh->SetVisibility(true); }

private:
    // 嘗試取得 cells：先 VoxelAssetDirect，再載入 VoxFilePath
    bool LoadCells(TArray<FVoxelCell>& OutCells) const;
};
