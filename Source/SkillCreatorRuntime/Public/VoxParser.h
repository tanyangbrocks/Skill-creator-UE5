#pragma once
#include "CoreMinimal.h"
#include "VoxelAsset.h"

// .vox 格式解析器（plan-voxelization.md §七）
// 支援 MagicaVoxel .vox 150 格式
//
// 調色盤公約（不需 DataAsset）：MagicaVoxel palette index 直接對應 EMaterialType：
//   0=Air（保留/空），1=Stone，2=Dirt，3=Grass，4=Sand，5=Water，
//   6=Lava，7=Wood，8=Leaves，9=Ore_Iron，10=Ore_Gold，…（依序）
// 在 MagicaVoxel 中上色時，用第 N 格 = EMaterialType(N) 即可。
namespace VoxParser
{
    // 從 .vox 二進位資料解析體素，輸出 FVoxelCell 陣列（已過濾 Air）
    // 座標軸轉換：MagicaVoxel（X=右,Y=深,Z=上）→ Voxel 世界（X=右,Y=下,Z=南）
    // OutBoundsMax：SIZE chunk 維度（tile 單位，local 空間）
    bool Parse(const TArray<uint8>& Raw,
               TArray<FVoxelCell>& OutCells,
               FIntVector& OutBoundsMax);
}
