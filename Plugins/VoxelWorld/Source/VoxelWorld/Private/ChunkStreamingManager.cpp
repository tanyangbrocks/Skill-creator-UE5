#include "ChunkStreamingManager.h"
#include "TileWorld3D.h"
#include "WorldScale.h"
#include "Misc/Paths.h"

void FChunkStreamingManager::Init(int32 WorldW, int32 WorldH, int32 WorldD,
                                    int32 Seed, const FString& InWorldDir)
{
    WorldDir = InWorldDir;
    DiskAttempted.Empty();
    MapGen.InitTerrainParams(WorldW, WorldH, WorldD, Seed);

    // 2026-06-23：換世界（AVoxelWorldActor::ReinitializeForWorld）也會呼叫到這裡。
    // InitTerrainParams() 只重設地表水池佈局，沒有清 GeneratedChunks——這個 TSet 紀錄
    // 「哪些 chunk 座標已經生成過」，不會因為換了 seed 自動失效，玩家進新世界走到舊世界
    // 探索過的同一個相對座標時，IsChunkGenerated() 會誤判「已生成」，導致那個座標的地形
    // 永遠生不出來（一直是空氣，TileWorld 已經被清空但沒人觸發重新生成）。
    MapGen.ResetGenerationState();
}

void FChunkStreamingManager::Tick(FTileWorld3D& World, int32 CX, int32 CY, int32 CZ,
                                   int32 StreamRadius, int32 CARadius)
{
    // 1. Promote async-generated chunks into the world.
    MapGen.ApplyPendingChunks(World);

    // 2. For each chunk slot in the streaming radius, load from disk or
    //    schedule async terrain generation if not already present.
    if (!WorldDir.IsEmpty())
    {
        const TMap<FIntVector, FChunk3D*>& Active = World.GetActiveChunks();
        for (int32 dx = -StreamRadius; dx <= StreamRadius; ++dx)
        for (int32 dy = -StreamRadius; dy <= StreamRadius; ++dy)
        for (int32 dz = -StreamRadius; dz <= StreamRadius; ++dz)
        {
            const FIntVector CC(CX + dx, CY + dy, CZ + dz);
            if (Active.Contains(CC)) continue;           // already loaded
            if (DiskAttempted.Contains(CC)) continue;    // already tried disk

            DiskAttempted.Add(CC);
            if (!World.TryLoadChunk(CC.X, CC.Y, CC.Z, WorldDir))
            {
                // No save file → generate procedurally.
                MapGen.EnsureChunksAround(World, CC.X, CC.Y, CC.Z, 0, 1);
            }
        }
    }
    else
    {
        // No save directory configured — just generate.
        MapGen.EnsureChunksAround(World, CX, CY, CZ, StreamRadius);
    }

    // 3. Run CA simulation.
    World.Tick(CX, CY, CZ, CARadius);
}

void FChunkStreamingManager::SaveAndEvict(FTileWorld3D& World,
                                           int32 CX, int32 CY, int32 CZ,
                                           int32 KeepRadius)
{
    // EvictFarChunks saves dirty chunks before removing them from memory.
    TArray<FIntVector> Evicted = World.EvictFarChunks(CX, CY, CZ, KeepRadius, WorldDir);

    // Forget disk-attempted status for evicted chunks so they can be
    // reloaded from disk next time they enter the streaming radius.
    for (const FIntVector& CC : Evicted)
        DiskAttempted.Remove(CC);

    // 清除 DiskAttempted 中超出 KeepRadius 的殭屍條目
    // （非同步生成從未完成的 chunk 永遠不在 Active，EvictFarChunks 不會處理它們）
    TArray<FIntVector> Stale;
    for (const FIntVector& CC : DiskAttempted)
        if (FMath::Abs(CC.X - CX) > KeepRadius ||
            FMath::Abs(CC.Y - CY) > KeepRadius ||
            FMath::Abs(CC.Z - CZ) > KeepRadius)
            Stale.Add(CC);
    for (const FIntVector& CC : Stale)
        DiskAttempted.Remove(CC);
}
