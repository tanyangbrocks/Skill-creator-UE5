#pragma once
#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "MaterialType.h"
#include "SurfaceWaterPool.h"

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

    // 2026-06-23：換世界時呼叫（AVoxelWorldActor::ReinitializeForWorld）。InitTerrainParams()
    // 只重設 WorldW/H/D/Seed + 地表水池佈局，沒有清 GeneratedChunks/InFlightChunks/
    // ReadyQueue——這三個追蹤的是「哪些 chunk 座標已經生成過/正在背景生成/算完等待套用」，
    // 不會因為換了 seed 就自動失效。沒清的後果：① GeneratedChunks 殘留舊世界探索過的座標，
    // 玩家進新世界走到同樣的相對座標時 IsChunkGenerated() 誤判「已生成」可能跳過真正生成；
    // ② 若換世界時舊世界還有背景執行緒在算 chunk，算完後會用舊 seed 的資料寫進新世界
    // （ReadyQueue 不分世界）。清空這三個狀態，舊世界的背景任務结果之後 Apply 時會寫進
    // 已經被清空的 TileWorld，視覺上不會出現，但 GeneratedChunks 誤判才是真正會讓玩家
    // 撞到「這裡明明探索過，現在卻是空氣」的根因。
    void ResetGenerationState();

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

    // 是否還有已排程但尚未套用的背景 thread chunk（CreateWorld 同步預生成等待用）
    bool HasPendingChunks() const { return !InFlightChunks.IsEmpty(); }

    // 還有幾個 chunk 在背景執行緒算、尚未套用（UI 進度顯示用，例如 GameFlowWidget 的世界生成 loading）
    int32 GetPendingChunkCount() const { return InFlightChunks.Num(); }

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

    // 地表水池佈局（InitTerrainParams 時 Initialize+Prepare 一次；之後純讀取，
    // EnsureChunksAround 把 GetPools() 的拷貝傳進背景 thread，thread-safe）
    FSurfaceWaterPool WaterPool;

    TSet<FIntVector>                         GeneratedChunks;
    TSet<FIntVector>                         InFlightChunks;
    TQueue<FPendingChunk, EQueueMode::Mpsc>  ReadyQueue;

    // 純函數（thread-safe）：計算一個 chunk 的所有 tile（含礦脈/裝飾/可行進洞穴/地表水池覆寫）
    static void ComputeChunkData(FIntVector CC, int32 Seed, int32 Height,
                                  const TArray<FSurfaceWaterPool::FPoolDesc>& WaterPools,
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
