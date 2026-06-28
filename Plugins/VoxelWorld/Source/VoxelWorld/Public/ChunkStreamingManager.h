#pragma once
#include "CoreMinimal.h"
#include "MapGenerator3D.h"

class FTileWorld3D;

// Wraps FMapGenerator3D and adds disk-load-first chunk streaming.
// On EnsureChunksAround: tries TileWorld::TryLoadChunk() before scheduling
// async terrain generation. On eviction: saves dirty chunks to disk.
class VOXELWORLD_API FChunkStreamingManager
{
public:
    FChunkStreamingManager() = default;

    // Must be called before Tick. WorldDir = absolute path to save folder.
    void Init(int32 WorldW, int32 WorldH, int32 WorldD, int32 Seed,
              const FString& InWorldDir);

    // Per-frame: apply pending async chunks, disk-load or generate nearby
    // chunks, run CA sim.
    void Tick(FTileWorld3D& World, int32 CX, int32 CY, int32 CZ,
              int32 StreamRadius, int32 CARadius);

    // Save dirty chunks and evict far ones (saving on eviction).
    void SaveAndEvict(FTileWorld3D& World, int32 CX, int32 CY, int32 CZ,
                      int32 KeepRadius);

    const FString& GetWorldDir() const { return WorldDir; }

    // CreateWorld 出生點預生成需要直接驅動 MapGen（EnsureChunksAround/ApplyPendingChunks）
    FMapGenerator3D& GetMapGenerator() { return MapGen; }

private:
    FMapGenerator3D  MapGen;
    FString          WorldDir;
    TSet<FIntVector> DiskAttempted;   // chunks already tried from disk (hit or miss)
    int32            DebugLoadLogCount = 0;  // throttle diagnostic logs to first 30 hits
};
