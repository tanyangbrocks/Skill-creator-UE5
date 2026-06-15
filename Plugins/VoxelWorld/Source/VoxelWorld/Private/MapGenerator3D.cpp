#include "MapGenerator3D.h"
#include "TileWorld3D.h"
#include "Chunk3D.h"
#include "WorldScale.h"
#include "Tasks/Task.h"

THIRD_PARTY_INCLUDES_START
#include "FastNoiseLite.h"
THIRD_PARTY_INCLUDES_END

// ============================================================
// 構造 / 解構
// ============================================================

FMapGenerator3D::FMapGenerator3D()  = default;
FMapGenerator3D::~FMapGenerator3D() = default;

void FMapGenerator3D::InitTerrainParams(int32 Width, int32 Height, int32 Depth, int32 Seed)
{
    WorldW    = Width;
    WorldH    = FMath::Max(Height, 16);
    WorldD    = Depth;
    WorldSeed = Seed;
}

// ============================================================
// 高度查詢（供 Spawn 計算 / 實體落地）
// ============================================================

int32 FMapGenerator3D::GetHeightAt(int32 WorldX, int32 WorldZ) const
{
    FastNoiseLite N;
    N.SetSeed(WorldSeed);
    N.SetFrequency(0.005f);
    N.SetFractalType(FastNoiseLite::FractalType::FBm);
    N.SetFractalOctaves(7);
    N.SetFractalLacunarity(2.f);
    N.SetFractalGain(0.5f);
    float v = N.GetNoise((float)WorldX, (float)WorldZ);
    return FMath::Clamp((int32)(WorldH * 0.4f + v * WorldH * 0.15f), 2, WorldH - 4);
}

// ============================================================
// 靜態地形計算（純函數，可在任意 thread 呼叫）
// ============================================================
// 座標約定：Y=0 為世界頂部（天空），Y 增大方向為向下
// SurfaceY：地表 tile（Grass）的 Y 值
//   Y < SurfaceY           → Air（地面以上）
//   Y == SurfaceY          → Grass
//   SurfaceY < Y <= +3     → Dirt
//   Y > SurfaceY + 3       → Stone（可被洞穴雕刻）
//   Y == WorldH - 1        → Stone（基岩，不可雕刻）

void FMapGenerator3D::ComputeChunkData(FIntVector CC, int32 Seed, int32 Height,
                                        TArray<FTileCell>& OutCells)
{
    // 高度圖 noise（FBm 7 octave）
    FastNoiseLite HN;
    HN.SetSeed(Seed);
    HN.SetFrequency(0.005f);
    HN.SetFractalType(FastNoiseLite::FractalType::FBm);
    HN.SetFractalOctaves(7);
    HN.SetFractalLacunarity(2.f);
    HN.SetFractalGain(0.5f);

    // 洞穴 noise（兩個偏移場：a²+b² < 門檻 → 細長蟲洞）
    FastNoiseLite CA, CB;
    CA.SetSeed(Seed ^ 0xDEAD1234);
    CA.SetFrequency(0.04f);
    CB.SetSeed(Seed ^ 0xBEEF5678);
    CB.SetFrequency(0.04f);

    // 寬洞穴 noise
    FastNoiseLite WN;
    WN.SetSeed(Seed ^ 0xCAFEBABE);
    WN.SetFrequency(0.02f);

    constexpr int32 S = WorldScale::ChunkSize;
    OutCells.SetNumZeroed(S * S * S);

    for (int32 lz = 0; lz < S; ++lz)
    for (int32 ly = 0; ly < S; ++ly)
    for (int32 lx = 0; lx < S; ++lx)
    {
        int32 wx = CC.X * S + lx;
        int32 wy = CC.Y * S + ly;
        int32 wz = CC.Z * S + lz;

        // 基岩保護
        if (wy >= Height - 1)
        {
            int32 Idx = lz*S*S + ly*S + lx;
            OutCells[Idx].MaterialID = (uint8)EMaterialType::Stone;
            continue;
        }

        float Hv = HN.GetNoise((float)wx, (float)wz);  // -1..1
        int32 SurfaceY = FMath::Clamp(
            (int32)(Height * 0.4f + Hv * Height * 0.15f), 2, Height - 4);

        uint8 MatID = 0;  // Air
        if (wy == SurfaceY)
            MatID = (uint8)EMaterialType::Grass;
        else if (wy > SurfaceY && wy <= SurfaceY + 3)
            MatID = (uint8)EMaterialType::Dirt;
        else if (wy > SurfaceY + 3)
            MatID = (uint8)EMaterialType::Stone;
        // wy < SurfaceY → Air（保持 0）

        // 洞穴雕刻（只雕刻地下石頭/泥土，保留地表 + 基岩）
        if (MatID != 0 && wy > SurfaceY && wy < Height - 1)
        {
            float a = CA.GetNoise((float)wx, (float)wy, (float)wz);
            float b = CB.GetNoise((float)wx + 1000.f, (float)wy + 1000.f, (float)wz + 1000.f);
            float w = WN.GetNoise((float)wx, (float)wy * 0.5f, (float)wz);

            if (a*a + b*b < 0.02f || w > 0.75f)
                MatID = 0;  // 雕空
        }

        int32 Idx = lz*S*S + ly*S + lx;
        OutCells[Idx].MaterialID = MatID;
    }
}

// ============================================================
// 非同步 chunk 排程
// ============================================================

void FMapGenerator3D::EnsureChunksAround(FTileWorld3D& World,
                                           int32 cx, int32 cy, int32 cz,
                                           int32 Radius, int32 MaxPerCall)
{
    int32 Launched = 0;
    for (int32 dcx = -Radius; dcx <= Radius && Launched < MaxPerCall; ++dcx)
    for (int32 dcy = -Radius; dcy <= Radius && Launched < MaxPerCall; ++dcy)
    for (int32 dcz = -Radius; dcz <= Radius && Launched < MaxPerCall; ++dcz)
    {
        FIntVector CC(cx+dcx, cy+dcy, cz+dcz);
        if (GeneratedChunks.Contains(CC) || InFlightChunks.Contains(CC)) continue;

        // 不生成超出世界高度的 chunk
        if (WorldH > 0)
        {
            int32 ChunkTopY = CC.Y * WorldScale::ChunkSize;
            if (ChunkTopY >= WorldH || CC.Y < 0) continue;
        }

        InFlightChunks.Add(CC);
        ++Launched;

        int32 Seed   = WorldSeed;
        int32 Height = WorldH;
        TQueue<FPendingChunk, EQueueMode::Mpsc>* Queue = &ReadyQueue;

        UE::Tasks::Launch(UE_SOURCE_LOCATION,
            [CC, Seed, Height, Queue]()
            {
                FPendingChunk P;
                P.Coord = CC;
                ComputeChunkData(CC, Seed, Height, P.Cells);
                Queue->Enqueue(MoveTemp(P));
            });
    }
}

// ============================================================
// 主執行緒：套用已算完的 chunk
// ============================================================

void FMapGenerator3D::ApplyPendingChunks(FTileWorld3D& World, int32 MaxPerFrame)
{
    constexpr int32 S = WorldScale::ChunkSize;
    int32 Applied = 0;
    FPendingChunk Pending;
    while (Applied < MaxPerFrame && ReadyQueue.Dequeue(Pending))
    {
        FIntVector CC = Pending.Coord;
        InFlightChunks.Remove(CC);
        GeneratedChunks.Add(CC);

        FChunk3D* C = World.GetOrCreateChunk(CC);
        check(Pending.Cells.Num() == S*S*S);
        FMemory::Memcpy(C->Cells, Pending.Cells.GetData(), sizeof(C->Cells));

        // 標記整個 chunk 的 dirty AABB → 觸發 Mesh 重建
        C->MarkDirty(0,   0,   0  );
        C->MarkDirty(S-1, S-1, S-1);

        ++Applied;
    }
}

// ============================================================
// 出生點計算
// ============================================================

FSpawnData FMapGenerator3D::ComputeSpawnPoint(const FTileWorld3D& /*World*/) const
{
    FSpawnData Out;
    int32 SpawnX = WorldW > 0 ? WorldW / 2 : 0;
    int32 SpawnZ = WorldD > 0 ? WorldD / 2 : 0;
    int32 SY     = GetHeightAt(SpawnX, SpawnZ);
    Out.PlayerSpawn = FIntVector(SpawnX, SY - 1, SpawnZ);  // 站在地表正上方
    return Out;
}
