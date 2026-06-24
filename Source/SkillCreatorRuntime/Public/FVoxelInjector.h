#pragma once
#include "CoreMinimal.h"
#include "VoxelAsset.h"
#include "WorldTypes.h"

class FTileWorld3D;

// 執行期體素注入（plan-voxelization.md §八）
// 將 UVoxelAsset 的 tile 資料注入到 FTileWorld3D（Route B 毀壞時呼叫）
struct FVoxelInjector
{
    // 注入並回傳世界 tile 座標的包圍盒（供後續 TriggerVoxelDestruction 使用）
    // WorldH：TileWorld->Height（用於 UE5↔Voxel 座標轉換）
    static void Inject(
        const UVoxelAsset*   Asset,
        const FTransform&    WorldTransform,
        FTileWorld3D*        World,
        int32                WorldH,
        FIntVector&          OutBoundsMin,
        FIntVector&          OutBoundsMax);
};
