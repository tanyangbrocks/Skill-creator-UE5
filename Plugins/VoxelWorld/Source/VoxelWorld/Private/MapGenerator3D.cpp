#include "MapGenerator3D.h"
#include "TileWorld3D.h"
#include "Chunk3D.h"
#include "WorldScale.h"
#include "Tasks/Task.h"
#include "SurfaceWaterPool.h"

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
    N.SetFractalOctaves(4);      // 4 octaves: 最高頻率 0.04 → 週期 25 格，無微凸起
    N.SetFractalLacunarity(2.f);
    N.SetFractalGain(0.5f);
    float v = N.GetNoise((float)WorldX, (float)WorldZ);
    return FMath::Clamp((int32)(WorldH * 0.4f + v * WorldH * 0.30f), 4, WorldH - 8);
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
    // 高度圖 noise（FBm 4 octave：平滑大地形，減少微凸起避免格狀感）
    FastNoiseLite HN;
    HN.SetSeed(Seed);
    HN.SetFrequency(0.005f);
    HN.SetFractalType(FastNoiseLite::FractalType::FBm);
    HN.SetFractalOctaves(4);
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
            (int32)(Height * 0.4f + Hv * Height * 0.30f), 4, Height - 8);

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

    EnsureWalkableCavesInChunk(OutCells);
    PlaceOreVeinsInChunk(OutCells, CC, Seed, Height);
    AddDecorInChunk(OutCells, CC, Seed);
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

// ============================================================
// 礦脈生成（per-chunk 確定性 BFS blob）
// ============================================================

void FMapGenerator3D::PlaceOreVeinsInChunk(TArray<FTileCell>& Cells,
                                             FIntVector CC, int32 Seed, int32 WorldH)
{
    constexpr int32 S   = WorldScale::ChunkSize;
    const    int32  WY0 = CC.Y * S;

    // LCG 偽隨機（per-chunk 確定性，不需 Random 物件）
    uint32 St = (uint32)(CC.X * 1664525u ^ CC.Y * 22695477u ^ CC.Z * 1013904223u ^ (uint32)Seed);
    auto RandF = [&]() -> float  { St = St * 1664525u + 1013904223u; return float(St & 0xFFFFu) / 65535.f; };
    auto RandN = [&](int32 N) -> int32 { return N > 0 ? (int32)((St = St * 1664525u + 1013904223u) % (uint32)N) : 0; };

    struct FVeinCfg { EMaterialType Mat; float YMinR; float YMaxR; int32 Blobs; int32 MaxSz; };
    const FVeinCfg Cfgs[] = {
        { EMaterialType::Ore_Coal,         0.28f, 0.62f, 2, 9 },
        { EMaterialType::Ore_Copper,       0.44f, 0.78f, 2, 6 },
        { EMaterialType::Ore_Iron,         0.58f, 0.90f, 1, 5 },
        { EMaterialType::Ore_MagicCrystal, 0.74f, 0.95f, 1, 3 },
    };
    static const int32 D6[6][3] = {{0,1,0},{0,-1,0},{1,0,0},{-1,0,0},{0,0,1},{0,0,-1}};

    for (const FVeinCfg& Cfg : Cfgs)
    {
        int32 LYMin = FMath::Max(0,   FMath::RoundToInt(WorldH * Cfg.YMinR) - WY0);
        int32 LYMax = FMath::Min(S-1, FMath::RoundToInt(WorldH * Cfg.YMaxR) - WY0);
        if (LYMin > LYMax) continue;

        for (int32 Blob = 0; Blob < Cfg.Blobs; ++Blob)
        {
            int32 lx = RandN(S), ly = LYMin + RandN(LYMax - LYMin + 1), lz = RandN(S);
            int32 SI = lz*S*S + ly*S + lx;
            if (Cells[SI].MaterialID != (uint8)EMaterialType::Stone) continue;

            Cells[SI].MaterialID = (uint8)Cfg.Mat;
            int32 Placed = 1;
            TArray<int32> BfsQ;
            BfsQ.Reserve(Cfg.MaxSz);
            BfsQ.Add(SI);

            for (int32 Qi = 0; Qi < BfsQ.Num() && Placed < Cfg.MaxSz; ++Qi)
            {
                int32 I = BfsQ[Qi];
                int32 ix = I % S, iy = (I / S) % S, iz = I / (S * S);
                for (const auto& D : D6)
                {
                    if (Placed >= Cfg.MaxSz) break;
                    int32 nx = ix+D[0], ny = iy+D[1], nz = iz+D[2];
                    if ((uint32)nx>=(uint32)S||(uint32)ny>=(uint32)S||(uint32)nz>=(uint32)S) continue;
                    int32 NI = nz*S*S + ny*S + nx;
                    if (Cells[NI].MaterialID != (uint8)EMaterialType::Stone) continue;
                    if (RandF() < 0.65f) { Cells[NI].MaterialID = (uint8)Cfg.Mat; BfsQ.Add(NI); ++Placed; }
                }
            }
        }
    }
}

// ============================================================
// 可行進洞穴保證（per-chunk，確保洞穴地板有 >= PlayerH 格淨空）
// ============================================================

void FMapGenerator3D::EnsureWalkableCavesInChunk(TArray<FTileCell>& Cells)
{
    constexpr int32 S        = WorldScale::ChunkSize;
    constexpr int32 MinClear = WorldScale::PlayerH;  // 2

    for (int32 lz = 0; lz < S; ++lz)
    for (int32 lx = 0; lx < S; ++lx)
    {
        for (int32 ly = 1; ly < S - 1; ++ly)
        {
            if (Cells[lz*S*S + ly*S + lx].MaterialID     != 0) continue;  // not air
            if (Cells[lz*S*S + (ly+1)*S + lx].MaterialID == 0) continue;  // below is air → not a floor

            // Count headroom upward (toward smaller ly)
            int32 Clear = 1;
            for (int32 up = ly - 1; up >= 0 && Cells[lz*S*S + up*S + lx].MaterialID == 0; --up)
                ++Clear;

            if (Clear < MinClear)
            {
                int32 TopAir = ly - (Clear - 1);
                int32 ToCarve = MinClear - Clear;
                for (int32 i = 1; i <= ToCarve; ++i)
                {
                    int32 ty = TopAir - i;
                    if (ty < 0) break;
                    Cells[lz*S*S + ty*S + lx].MaterialID = 0;
                }
            }
        }
    }
}

// ============================================================
// 裝飾（per-chunk 確定性：鐘乳石從天花板往下 + 小水坑在洞穴地板）
// ============================================================

void FMapGenerator3D::AddDecorInChunk(TArray<FTileCell>& Cells, FIntVector CC, int32 Seed)
{
    constexpr int32 S = WorldScale::ChunkSize;
    uint32 St = (uint32)(CC.X * 1013904223u ^ CC.Y * 1664525u ^ CC.Z * 22695477u ^ (uint32)Seed ^ 0xBEEFu);
    auto RandF = [&]() -> float  { St = St * 1664525u + 1013904223u; return float(St & 0xFFFFu) / 65535.f; };
    auto RandN = [&](int32 N) -> int32 { return N > 0 ? (int32)((St = St * 1664525u + 1013904223u) % (uint32)N) : 0; };

    // 鐘乳石：air tile，上方（ly-1）是 Stone → 往下（+Y）生長
    for (int32 lz = 0; lz < S; ++lz)
    for (int32 ly = 1; ly < S - 3; ++ly)
    for (int32 lx = 0; lx < S; ++lx)
    {
        if (Cells[lz*S*S + ly*S + lx].MaterialID     != 0)                       continue;
        if (Cells[lz*S*S + (ly-1)*S + lx].MaterialID != (uint8)EMaterialType::Stone) continue;
        if (RandF() >= 0.04f) continue;

        int32 Len = 1 + RandN(3);
        for (int32 i = 0; i < Len; ++i)
        {
            int32 ty = ly + i;
            if (ty >= S) break;
            int32 TI = lz*S*S + ty*S + lx;
            if (Cells[TI].MaterialID != 0) break;
            Cells[TI].MaterialID = (uint8)EMaterialType::Stone;
        }
    }

    // 小水坑：air tile，下方（ly+1）是 Stone → 在此 air 位置放 Water
    for (int32 lz = 1; lz < S-1; ++lz)
    for (int32 ly = 1; ly < S-1; ++ly)
    for (int32 lx = 1; lx < S-1; ++lx)
    {
        if (Cells[lz*S*S + ly*S + lx].MaterialID     != 0)                       continue;
        if (Cells[lz*S*S + (ly+1)*S + lx].MaterialID != (uint8)EMaterialType::Stone) continue;
        if (RandF() >= 0.03f) continue;

        for (int32 dz = -1; dz <= 1; ++dz)
        for (int32 dx = -1; dx <= 1; ++dx)
        {
            int32 px = lx + dx, pz = lz + dz;
            if ((uint32)px >= (uint32)S || (uint32)pz >= (uint32)S) continue;
            int32 PIdx = pz*S*S + ly*S + px;
            if (Cells[PIdx].MaterialID == 0)
                Cells[PIdx].MaterialID = (uint8)EMaterialType::Water;
        }
    }
}

// ============================================================
// FloodFill3D（BFS 可達空格集合，主執行緒用）
// ============================================================

TSet<FIntVector> FMapGenerator3D::FloodFill3D(FTileWorld3D& World, FIntVector Start,
                                               FIntVector BMin, FIntVector BMax)
{
    TSet<FIntVector> Visited;
    if (World.GetTile(Start.X, Start.Y, Start.Z) != EMaterialType::Air) return Visited;

    TQueue<FIntVector> Queue;
    Queue.Enqueue(Start);
    Visited.Add(Start);

    static const FIntVector Dirs6[] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    while (!Queue.IsEmpty())
    {
        FIntVector Pos;
        Queue.Dequeue(Pos);
        for (const FIntVector& D : Dirs6)
        {
            FIntVector N = Pos + D;
            if (N.X < BMin.X || N.X > BMax.X ||
                N.Y < BMin.Y || N.Y > BMax.Y ||
                N.Z < BMin.Z || N.Z > BMax.Z) continue;
            if (World.GetTile(N.X, N.Y, N.Z) != EMaterialType::Air) continue;
            bool bAlreadyIn = false;
            Visited.Add(N, &bAlreadyIn);
            if (!bAlreadyIn) Queue.Enqueue(N);
        }
    }
    return Visited;
}

// ============================================================
// PostProcessRegion（連通性後處理，主執行緒，初始生成區全 Apply 後呼叫）
// ============================================================

void FMapGenerator3D::PostProcessRegion(FTileWorld3D& World,
                                         FIntVector ChunkMin, FIntVector ChunkMax)
{
    constexpr int32 S = WorldScale::ChunkSize;
    FIntVector WMin(ChunkMin.X * S, ChunkMin.Y * S, ChunkMin.Z * S);
    FIntVector WMax((ChunkMax.X+1) * S - 1, (ChunkMax.Y+1) * S - 1, (ChunkMax.Z+1) * S - 1);
    if (WorldW > 0) WMax.X = FMath::Min(WMax.X, WorldW - 1);
    if (WorldH > 0) WMax.Y = FMath::Min(WMax.Y, WorldH - 1);
    if (WorldD > 0) WMax.Z = FMath::Min(WMax.Z, WorldD - 1);

    int32 MidX = (WMin.X + WMax.X) / 2;
    int32 MidZ = (WMin.Z + WMax.Z) / 2;
    int32 SurfY = GetHeightAt(MidX, MidZ);

    // 找到一個確實是 Air 的出發點
    FIntVector Start(MidX, FMath::Max(WMin.Y, SurfY - 1), MidZ);
    while (Start.Y <= WMax.Y && World.GetTile(Start.X, Start.Y, Start.Z) != EMaterialType::Air)
        ++Start.Y;
    if (Start.Y > WMax.Y) return;

    TSet<FIntVector> Visited = FloodFill3D(World, Start, WMin, WMax);
    int32 CaveDeepY = WMin.Y + FMath::RoundToInt((WMax.Y - WMin.Y) * 0.35f);

    // 每隔 4 格掃描一列：若有孤立 Air，從地表打通垂直通道
    int32 XFrom = WMin.X + (WMax.X - WMin.X) / 4;
    int32 XTo   = WMin.X + (WMax.X - WMin.X) * 3 / 4;
    int32 ZFrom = WMin.Z + (WMax.Z - WMin.Z) / 4;
    int32 ZTo   = WMin.Z + (WMax.Z - WMin.Z) * 3 / 4;

    for (int32 x = XFrom; x <= XTo; x += 4)
    for (int32 z = ZFrom; z <= ZTo; z += 4)
    {
        int32 ColSurf = GetHeightAt(x, z);
        for (int32 y = CaveDeepY; y <= WMax.Y - 2; ++y)
        {
            if (World.GetTile(x, y, z) != EMaterialType::Air) continue;
            if (Visited.Contains(FIntVector(x, y, z))) continue;

            // 孤立洞穴：從地表 Y 向下打通通道
            for (int32 sy = FMath::Max(WMin.Y, ColSurf); sy <= y; ++sy)
                World.SetTile(x, sy, z, EMaterialType::Air);
            break;
        }
    }

    // 地表水池（SurfaceWaterPool）
    FSurfaceWaterPool WaterPool;
    WaterPool.Initialize(WorldSeed, WorldW, WorldH, WorldD);
    WaterPool.Prepare(World, ChunkMin, ChunkMax);
    WaterPool.PlaceInWorld(World, ChunkMin, ChunkMax);
}
