#pragma once
#include "CoreMinimal.h"
#include "VoxelAsset.h"

// .vox 格式解析器（plan-voxelization.md §七）
// 支援 MagicaVoxel .vox 150 格式
namespace VoxParser
{
    // 從 .vox 二進位資料 + 調色盤映射，輸出 FVoxelCell 陣列（已過濾 Air）
    // 座標軸轉換：MagicaVoxel（X=右,Y=深,Z=上）→ Voxel 世界（X=右,Y=下,Z=南）
    // OutBoundsMax：SIZE chunk 回報的 x/y/z 最大值（tile 單位 local 空間）
    bool Parse(const TArray<uint8>& Raw,
               const UVoxelMaterialPalette* Palette,
               TArray<FVoxelCell>& OutCells,
               FIntVector& OutBoundsMax);
}
