#pragma once
#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "MaterialType.h"

class FTileWorld3D;

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

    // 連通性後處理（主執行緒，初始 chunk 群全部 Apply 後呼叫）
    // FloodFill3D 確認可達空間，對孤立洞穴打通垂直通道
    void PostProcessRegion(FTileWorld3D& World, FIntVector ChunkMin, FIntVector ChunkMax);

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

    // 純函數（thread-safe）：計算一個 chunk 的所有 tile（含礦脈/裝飾/可行進洞穴）
    static void ComputeChunkData(FIntVector CC, int32 Seed, int32 Height,
                                  TArray<FTileCell>& OutCells);

    // ── per-chunk 後處理（靜態，thread-safe，僅操作 OutCells buffer）──────
    static void PlaceOreVeinsInChunk(TArray<FTileCell>& Cells, FIntVector CC,
                                      int32 Seed, int32 WorldH);
    static void EnsureWalkableCavesInChunk(TArray<FTileCell>& Cells);
    static void AddDecorInChunk(TArray<FTileCell>& Cells, FIntVector CC, int32 Seed);

    // ── 主執行緒連通性（操作 FTileWorld3D）────────────────────────────────
    static TSet<FIntVector> FloodFill3D(FTileWorld3D& World, FIntVector Start,
                                         FIntVector BMin, FIntVector BMax);
};
