#pragma once
#include "CoreMinimal.h"
#include "VoxelAsset.h"
#include "WorldTypes.h"

class FTileWorld3D;

// 執行期體素注入（plan-voxelization.md §八）
struct FVoxelInjector
{
    // 主要路徑：直接傳入解析後的 FVoxelCell 陣列（runtime .vox 載入後使用）
    static void Inject(
        const TArray<FVoxelCell>& Cells,
        const FTransform&         WorldTransform,
        FTileWorld3D*             World,
        int32                     WorldH,
        FIntVector&               OutBoundsMin,
        FIntVector&               OutBoundsMax);

    // 備用路徑：傳入 UVoxelAsset（Editor 預建 DataAsset 時使用）
    static void Inject(
        const UVoxelAsset*  Asset,
        const FTransform&   WorldTransform,
        FTileWorld3D*       World,
        int32               WorldH,
        FIntVector&         OutBoundsMin,
        FIntVector&         OutBoundsMax);
};
