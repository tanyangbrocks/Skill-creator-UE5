#include "TilePathfinder.h"
#include "TileWorld3D.h"
#include "MaterialType.h"
#include "Algo/Reverse.h"

// ── 基礎查詢 ──────────────────────────────────────────────────────────────────

bool FTilePathfinder::IsPassable(const FTileWorld3D& W, int32 x, int32 y, int32 z)
{
    if (!W.InBounds(x, y, z)) return false;
    // 只有 Air 視為可通行（未來可擴充 Liquid 為游泳通行）
    return W.GetTile(x, y, z) == EMaterialType::Air;
}

bool FTilePathfinder::HasGround(const FTileWorld3D& W, int32 x, int32 y, int32 z)
{
    // Y 增大方向 = 向下；腳下 = y+1
    const int32 Below = y + 1;
    if (!W.InBounds(x, Below, z)) return true; // 超出下邊界視為有地
    return W.GetTile(x, Below, z) != EMaterialType::Air;
}

// ── 鄰居展開（3D：水平走 + 墜落 + 跳躍） ────────────────────────────────────

void FTilePathfinder::ExpandNeighbors(const FTileWorld3D& W,
                                       const FGridPos& Cur,
                                       int32 JumpH, int32 MaxFall,
                                       TArray<FGridPos>& Out)
{
    static constexpr int32 DX[4] = { 1, -1, 0,  0 };
    static constexpr int32 DZ[4] = { 0,  0, 1, -1 };

    for (int32 i = 0; i < 4; ++i)
    {
        const int32 NX = Cur.X + DX[i];
        const int32 NZ = Cur.Z + DZ[i];

        if (!IsPassable(W, NX, Cur.Y, NZ)) continue;

        // ① 平地走（目標格有地可站）
        if (HasGround(W, NX, Cur.Y, NZ))
        {
            Out.Add(FGridPos(NX, Cur.Y, NZ));
            continue;
        }

        // ② 墜落（目標格懸空，往下找落腳點）
        {
            bool bLanded = false;
            for (int32 fy = Cur.Y + 1; fy <= Cur.Y + MaxFall; ++fy)
            {
                if (!IsPassable(W, NX, fy, NZ)) break; // 撞牆，無法繼續
                if (HasGround(W, NX, fy, NZ))
                {
                    Out.Add(FGridPos(NX, fy, NZ));
                    bLanded = true;
                    break;
                }
            }
            if (bLanded) continue;
        }
    }

    // ③ 跳躍（往上 1..JumpH 格，再向 4 方向水平展開）
    // 頭頂必須有至少一格空間才能起跳
    if (JumpH > 0 && IsPassable(W, Cur.X, Cur.Y - 1, Cur.Z))
    {
        for (int32 jh = 1; jh <= JumpH; ++jh)
        {
            const int32 JY = Cur.Y - jh;
            if (!IsPassable(W, Cur.X, JY, Cur.Z)) break; // 頭頂撞牆

            for (int32 i = 0; i < 4; ++i)
            {
                const int32 NX = Cur.X + DX[i];
                const int32 NZ = Cur.Z + DZ[i];
                if (IsPassable(W, NX, JY, NZ) && HasGround(W, NX, JY, NZ))
                    Out.Add(FGridPos(NX, JY, NZ));
            }
        }
    }
}

// ── A* 主體 ──────────────────────────────────────────────────────────────────

TArray<FGridPos> FTilePathfinder::FindPath(const FTilePathRequest& Req)
{
    if (!Req.World || Req.Start == Req.Goal)
        return {};

    const FTileWorld3D& W = *Req.World;

    // A* open set：以 F = G + H 排序的優先佇列（TArray + 線性掃描，MaxIter<=1024 夠用）
    struct FAStarNode
    {
        FGridPos Pos;
        int32    G = 0;  // 到達成本
        int32    F = 0;  // G + H
    };

    TArray<FAStarNode>        OpenList;
    TMap<FGridPos, FGridPos>  CameFrom;
    TMap<FGridPos, int32>     GScore;

    const int32 H0 = Req.Start.ManhattanDistance(Req.Goal);
    OpenList.Add({ Req.Start, 0, H0 });
    GScore.Add(Req.Start, 0);

    int32 Iterations = 0;

    while (!OpenList.IsEmpty() && Iterations < Req.MaxIter)
    {
        ++Iterations;

        // 找最小 F 節點
        int32 BestIdx = 0;
        for (int32 i = 1; i < OpenList.Num(); ++i)
            if (OpenList[i].F < OpenList[BestIdx].F) BestIdx = i;

        FAStarNode Current = OpenList[BestIdx];
        OpenList.RemoveAtSwap(BestIdx, EAllowShrinking::No);

        if (Current.Pos == Req.Goal)
        {
            // 回溯路徑
            TArray<FGridPos> Path;
            FGridPos Cur = Req.Goal;
            while (Cur != Req.Start)
            {
                Path.Add(Cur);
                Cur = CameFrom[Cur];
            }
            Path.Add(Req.Start);
            Algo::Reverse(Path);
            return Path;
        }

        TArray<FGridPos> Neighbors;
        ExpandNeighbors(W, Current.Pos, Req.JumpHeight, Req.MaxFall, Neighbors);

        for (const FGridPos& N : Neighbors)
        {
            const int32 TentativeG = Current.G + 1;
            const int32* ExistingG = GScore.Find(N);
            if (ExistingG && *ExistingG <= TentativeG) continue;

            CameFrom.Add(N, Current.Pos);
            GScore.Add(N, TentativeG);

            const int32 F = TentativeG + N.ManhattanDistance(Req.Goal);
            OpenList.Add({ N, TentativeG, F });
        }
    }

    return {};  // 找不到路徑
}
