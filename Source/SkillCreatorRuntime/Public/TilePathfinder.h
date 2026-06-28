#pragma once
#include "CoreMinimal.h"
#include "GridPos.h"

class FTileWorld3D;

struct FTilePathRequest
{
    FGridPos            Start;
    FGridPos            Goal;
    const FTileWorld3D* World      = nullptr;
    int32               MaxIter    = 1024;
    int32               JumpHeight = 1;   // 最多往上跳幾格
    int32               MaxFall    = 8;   // 最多往下落幾格
};

// 3D tile-space A* 路徑尋找器。
// FindPath() 只讀 FTileWorld3D，無寫入，可在背景 thread 安全呼叫。
// 返回從 Start 到 Goal 的 tile 序列（含兩端）；找不到路徑時返回空陣列。
class SKILLCREATORRUNTIME_API FTilePathfinder
{
public:
    static TArray<FGridPos> FindPath(const FTilePathRequest& Req);

private:
    static bool IsPassable(const FTileWorld3D& W, int32 x, int32 y, int32 z);
    static bool HasGround (const FTileWorld3D& W, int32 x, int32 y, int32 z);
    static void ExpandNeighbors(const FTileWorld3D& W,
                                const FGridPos& Cur, int32 JumpH, int32 MaxFall,
                                TArray<FGridPos>& Out);
};
