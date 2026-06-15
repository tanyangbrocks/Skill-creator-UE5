#pragma once
#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "MaterialType.h"

class FTileWorld3D;

// M-3 簡化版：僅實作高度圖 + CA 洞穴雕刻
// ⚠️ M-5 TODO：加入礦脈佈置（BFS blob）、裝飾物（鐘乳石/水坑）、
//             連通性驗證（FloodFill3D）、ConcurrentBag noise 池優化、
//             非同步 chunk 生成完整取消機制

struct VOXELWORLD_API FSpawnData
{
    FIntVector PlayerSpawn = FIntVector(0, 0, 0);
};

class VOXELWORLD_API FMapGenerator3D
{
public:
    FMapGenerator3D();
    ~FMapGenerator3D();

    void  InitTerrainParams(int32 WorldWidth, int32 WorldHeight, int32 WorldDepth, int32 Seed);

    // 給定世界 XZ 座標，傳回地表 Y（Y=0 為頂部，值越大越靠近底部）
    int32 GetHeightAt(int32 WorldX, int32 WorldZ) const;

    // 非同步排程（在背景 thread 計算 chunk 資料，主執行緒 Apply）
    // 對玩家周圍 Radius 個 chunk 確保已排程或已生成
    void EnsureChunksAround(FTileWorld3D& World, int32 cx, int32 cy, int32 cz,
                             int32 Radius, int32 MaxPerCall = 4);

    // 主執行緒：把已算完的 chunk 資料寫入 World（每幀呼叫）
    void ApplyPendingChunks(FTileWorld3D& World, int32 MaxPerFrame = 4);

    FSpawnData ComputeSpawnPoint(const FTileWorld3D& World) const;

    bool IsChunkGenerated(FIntVector CC) const { return GeneratedChunks.Contains(CC); }

private:
    struct FPendingChunk
    {
        FIntVector        Coord;
        TArray<FTileCell> Cells;   // SizeCubed 個 FTileCell（背景 thread 計算）
    };

    int32 WorldW    = 0;
    int32 WorldH    = 64;
    int32 WorldD    = 0;
    int32 WorldSeed = 12345;

    TSet<FIntVector>                         GeneratedChunks;
    TSet<FIntVector>                         InFlightChunks;
    TQueue<FPendingChunk, EQueueMode::Mpsc>  ReadyQueue;

    // 純函數（thread-safe）：計算一個 chunk 的所有 tile
    static void ComputeChunkData(FIntVector CC, int32 Seed, int32 Height,
                                  TArray<FTileCell>& OutCells);
};
