#include "SurfaceWaterPool.h"
#include "TileWorld3D.h"
#include "WorldScale.h"

void FSurfaceWaterPool::Initialize(int32 Seed, int32 WorldWidth, int32 WorldHeight, int32 WorldDepth)
{
    WorldW = WorldWidth;
    WorldH = WorldHeight;
    WorldD = WorldDepth;
    Rng.Initialize(Seed ^ 0xABCDEF01);
    Pools.Empty();
}

void FSurfaceWaterPool::Prepare(const FTileWorld3D& World,
                                FIntVector ChunkMin, FIntVector ChunkMax)
{
    // 在 chunk 區域內隨機選取水池中心（必須在 chunk 內部，避開邊緣）
    Pools.Empty();
    const int32 RangeX = (ChunkMax.X - ChunkMin.X) * WorldScale::ChunkSize;
    const int32 RangeZ = (ChunkMax.Z - ChunkMin.Z) * WorldScale::ChunkSize;
    if (RangeX < PoolRadius * 4 || RangeZ < PoolRadius * 4) return;

    const int32 BaseX = ChunkMin.X * WorldScale::ChunkSize;
    const int32 BaseZ = ChunkMin.Z * WorldScale::ChunkSize;

    for (int32 i = 0; i < MaxPools; ++i)
    {
        FPoolDesc P;
        P.CX = BaseX + PoolRadius * 2 + Rng.RandRange(0, RangeX - PoolRadius * 4);
        P.CZ = BaseZ + PoolRadius * 2 + Rng.RandRange(0, RangeZ - PoolRadius * 4);

        // 掃描地表 Y（從頂部往下找第一個非 Air）
        P.SurfaceY = WorldH - 2;
        for (int32 y = 1; y < WorldH - 1; ++y)
        {
            if (World.GetTile(P.CX, y, P.CZ) != EMaterialType::Air)
            {
                P.SurfaceY = y;
                break;
            }
        }
        Pools.Add(P);
    }
}

void FSurfaceWaterPool::PlaceInWorld(FTileWorld3D& World,
                                     FIntVector ChunkMin, FIntVector ChunkMax)
{
    for (const FPoolDesc& P : Pools)
    {
        const int32 R2 = PoolRadius * PoolRadius;
        for (int32 dx = -PoolRadius; dx <= PoolRadius; ++dx)
        for (int32 dz = -PoolRadius; dz <= PoolRadius; ++dz)
        {
            const float DistSq = (float)(dx*dx + dz*dz);
            if (DistSq > R2) continue;

            // 碗形：邊緣挖淺、中心挖深
            const float FracDist = FMath::Sqrt(DistSq) / (float)PoolRadius;
            const int32 Depth    = FMath::Max(1, (int32)(MaxDepth * (1.f - FracDist)));

            const int32 wx = P.CX + dx;
            const int32 wz = P.CZ + dz;

            // 碗形填水：從地表 (SurfaceY) 往下 (Y 增大) 替換成 Water
            for (int32 dy = 0; dy < Depth; ++dy)
            {
                const int32 wy = P.SurfaceY + dy;
                if (!World.InBounds(wx, wy, wz)) continue;
                World.SetTile(wx, wy, wz, EMaterialType::Water);
            }

            // 碗底設為 Dirt（Water 層正下方的一格）
            const int32 BottomY = P.SurfaceY + Depth;
            if (World.InBounds(wx, BottomY, wz))
                World.SetTile(wx, BottomY, wz, EMaterialType::Dirt);
        }
    }
}

EMaterialType FSurfaceWaterPool::GetSurfaceOverride(int32 x, int32 z) const
{
    const int32 R2 = PoolRadius * PoolRadius;
    for (const FPoolDesc& P : Pools)
    {
        const int32 dx = x - P.CX;
        const int32 dz = z - P.CZ;
        if (dx*dx + dz*dz <= R2) return EMaterialType::Water;
    }
    return EMaterialType::Air;
}
