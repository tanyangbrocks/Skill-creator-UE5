#pragma once
#include "CoreMinimal.h"
#include "MaterialType.h"
#include "WorldScale.h"

// 16³ tile 儲存單元；純 POD 陣列 + 髒區 AABB + 逐幀 Updated 旗標
// M-6 Greedy Mesh 會掃描 bMeshNeedsRebuild 觸發網格重建
class VOXELWORLD_API FChunk3D
{
public:
    static constexpr int32 Size      = WorldScale::ChunkSize;       // 16
    static constexpr int32 SizeSq    = Size * Size;                  // 256
    static constexpr int32 SizeCubed = Size * Size * Size;           // 4096

    FIntVector ChunkCoord;

    FTileCell Cells[SizeCubed];          // 16 KB：所有 tile 資料
    bool      Updated[SizeCubed];        // 4 KB：防同幀重複模擬旗標

    bool  bDirty            = false;     // 本幀有 tile 變動（CA 模擬排程用）
    bool  bMeshNeedsRebuild = false;     // 渲染器（M-6）需要重建 Mesh
    bool  bNeedsSave        = false;     // 有未持久化的變動

    int32 DirtyMin[3] = {Size, Size, Size};
    int32 DirtyMax[3] = {-1,   -1,   -1  };

    explicit FChunk3D(FIntVector InCoord);

    // 線性索引：(x,y,z) in [0,Size)^3 → Cells[Idx]
    static int32 Idx(int32 x, int32 y, int32 z) { return z * SizeSq + y * Size + x; }

    FTileCell&       CellAt(int32 lx, int32 ly, int32 lz)       { return Cells[Idx(lx,ly,lz)]; }
    const FTileCell& CellAt(int32 lx, int32 ly, int32 lz) const { return Cells[Idx(lx,ly,lz)]; }

    bool IsDirty()                                  const { return bDirty; }
    bool IsUpdated(int32 lx, int32 ly, int32 lz)   const { return Updated[Idx(lx,ly,lz)]; }

    void MarkUpdated(int32 lx, int32 ly, int32 lz) { Updated[Idx(lx,ly,lz)] = true; }
    void MarkDirty(int32 lx, int32 ly, int32 lz);
    void FlagMeshRebuild();
    void ClearDirty();
    void ClearUpdated();
};
