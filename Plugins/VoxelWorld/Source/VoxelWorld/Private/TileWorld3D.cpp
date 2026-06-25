#include "TileWorld3D.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ElementalReactionTable.h"
#include "CaCellPacking.h"

// ============================================================
// 構造 / 解構
// ============================================================

FTileWorld3D::FTileWorld3D()
    : Rng(12345)
{}

FTileWorld3D::~FTileWorld3D()
{
    ClearAllChunks();
    GpuSim.Release();
}

void FTileWorld3D::ClearAllChunks()
{
    for (auto& Pair : Chunks)
        delete Pair.Value;
    Chunks.Empty();
    DirtyChunks.Empty();
    PendingNeighborDirty.Empty();
    OccupiedCells.Empty();
    CaReactionQueue.Empty();
}

// ============================================================
// GPU CA（M-10 Phase 1-5 完成）
// ============================================================

void FTileWorld3D::InitGpu()
{
    GpuSim.Initialize();
}

void FTileWorld3D::UpdateGpuOrigin(int32 wx, int32 wy, int32 wz)
{
    GpuSim.SetOrigin(wx, wy, wz);
}

bool FTileWorld3D::InGpuZone(int32 x, int32 y, int32 z) const
{
    if (!GpuSim.IsAvailable()) return false;
    return FMath::Abs(x - GpuSim.GetOriginX()) <= FCaGpuSimulator::ZoneW / 2 &&
           FMath::Abs(y - GpuSim.GetOriginY()) <= FCaGpuSimulator::ZoneH / 2 &&
           FMath::Abs(z - GpuSim.GetOriginZ()) <= FCaGpuSimulator::ZoneD / 2;
}

void FTileWorld3D::SetCellFromGpu(int32 x, int32 y, int32 z, uint32 PackedCell)
{
    // 只覆寫 GPU 會動到的欄位（MaterialID/CA_State/Variant），保留現有 Category——
    // GPU shader 完全不知道 ETileCategory，這個欄位的賦值權責不在這裡（見
    // docs/plan-m10-gpu-ca.md §五決策 1：目前全專案都還沒有任何地方真的填 Category）。
    FTileCell Cell = GetCell(x, y, z);
    Cell.MaterialID = CaCellPacking::UnpackMaterialID(PackedCell);
    Cell.CA_State    = CaCellPacking::UnpackTimer(PackedCell);
    Cell.Variant     = CaCellPacking::UnpackVariant(PackedCell);
    WriteCell(x, y, z, Cell);
}

// ── Phase 5 CVar：r.VoxelWorld.GpuCaAsync 1 開啟非同步 readback ──────────────
// 預設 0（sync）；在 PIE 測量完 Phase 4 確認 sync 是瓶頸後，設 1 啟用 Phase 5。
static TAutoConsoleVariable<int32> CVarGpuCaAsync(
    TEXT("r.VoxelWorld.GpuCaAsync"), 0,
    TEXT("0 = sync GPU CA (Phase 3/4)  1 = 1-frame async readback (Phase 5)"));

void FTileWorld3D::TickGpuZone()
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_CaGpu_TickTotal);

    // Phase 4 計時（每 10 秒 UE_LOG 一次，提供具體 ms 值）
    static double GpuTickLastLogTime = 0.0;
    const double TickStart = FPlatformTime::Seconds();

    const bool bAsync = CVarGpuCaAsync.GetValueOnGameThread() != 0;

    // ── Phase 5：先收上一幀 async 結果（若 GPU 已就緒）────────────────────────
    if (bAsync && GpuSim.HasPendingAsync())
    {
        GpuSim.TryCollectAsync([this](int32 wx, int32 wy, int32 wz, uint32 Cell)
        {
            SetCellFromGpu(wx, wy, wz, Cell);
        });
    }

    // Zone 中心是玩家所在格（UpdateGpuOrigin 設的），反推世界座標最小角。
    const int32 OX = GpuSim.GetOriginX() - FCaGpuSimulator::ZoneW / 2;
    const int32 OY = GpuSim.GetOriginY() - FCaGpuSimulator::ZoneH / 2;
    const int32 OZ = GpuSim.GetOriginZ() - FCaGpuSimulator::ZoneD / 2;
    constexpr int32 W = FCaGpuSimulator::ZoneW;
    constexpr int32 H = FCaGpuSimulator::ZoneH;
    constexpr int32 D = FCaGpuSimulator::ZoneD;

    // ── Phase 4：稀疏 chunk 打包（取代 4M 次 TMap::Find GetCell 呼叫）──────────
    // Zone = 128×256×128，chunk = 16³，zone 最多覆蓋 8×16×8 = 1024 個 chunk slot，
    // 其中大多數是 air chunk（不在 Chunks TMap），直接跳過。
    // 實際有地形的 chunk（~200-400 個）各自做一次 4096 元素的 flat array 讀取，
    // 大幅優於原版 4,194,304 次 TMap hash lookup。
    {
        QUICK_SCOPE_CYCLE_COUNTER(STAT_CaGpu_PackZone);

        TArray<uint32> PackedCells;
        PackedCells.SetNumZeroed(W * H * D);  // 預設 0 = Air = 無 physics bit

        const FIntVector ChunkMin = WorldToChunk(OX,         OY,         OZ);
        const FIntVector ChunkMax = WorldToChunk(OX + W - 1, OY + H - 1, OZ + D - 1);

        for (int32 CZ = ChunkMin.Z; CZ <= ChunkMax.Z; ++CZ)
        for (int32 CY = ChunkMin.Y; CY <= ChunkMax.Y; ++CY)
        for (int32 CX = ChunkMin.X; CX <= ChunkMax.X; ++CX)
        {
            FChunk3D* const* Found = Chunks.Find(FIntVector(CX, CY, CZ));
            if (!Found) continue;  // air chunk，PackedCells 對應位置已是 0
            FChunk3D* Chunk = *Found;

            const int32 WX0 = CX * FChunk3D::Size;
            const int32 WY0 = CY * FChunk3D::Size;
            const int32 WZ0 = CZ * FChunk3D::Size;

            // chunk 中重疊 zone 的 local 範圍
            const int32 lxMin = FMath::Max(0, OX - WX0);
            const int32 lxMax = FMath::Min(FChunk3D::Size - 1, OX + W - 1 - WX0);
            const int32 lyMin = FMath::Max(0, OY - WY0);
            const int32 lyMax = FMath::Min(FChunk3D::Size - 1, OY + H - 1 - WY0);
            const int32 lzMin = FMath::Max(0, OZ - WZ0);
            const int32 lzMax = FMath::Min(FChunk3D::Size - 1, OZ + D - 1 - WZ0);

            for (int32 lz = lzMin; lz <= lzMax; ++lz)
            for (int32 ly = lyMin; ly <= lyMax; ++ly)
            for (int32 lx = lxMin; lx <= lxMax; ++lx)
            {
                const FTileCell& Cell = Chunk->CellAt(lx, ly, lz);
                if (Cell.MaterialID == 0) continue;  // Air，保留預設 0

                const int32 zx = WX0 + lx - OX;
                const int32 zy = WY0 + ly - OY;
                const int32 zz = WZ0 + lz - OZ;
                PackedCells[zz * H * W + zy * W + zx] =
                    CaCellPacking::Pack(Cell.MaterialID, Cell.CA_State, Cell.Variant);
            }
        }

        // ── 踢 GPU 工作（sync 或 async 路徑）────────────────────────────────────
        if (bAsync)
        {
            // Phase 5：BeginAsync 踢出去，下一幀 TryCollectAsync 收結果（1-frame latency）
            GpuSim.BeginAsync(MoveTemp(PackedCells), W, H, D, static_cast<uint32>(Frame));
        }
        else
        {
            // Phase 3/4 sync 路徑（Upload+Simulate+Download 各自 FlushRenderingCommands）
            if (!GpuSim.Upload(PackedCells, W, H, D)) return;

            GpuSim.Simulate(static_cast<uint32>(Frame));

            GpuSim.Download([this, OX, OY, OZ](int32 lx, int32 ly, int32 lz, uint32 PackedCell)
            {
                SetCellFromGpu(OX + lx, OY + ly, OZ + lz, PackedCell);
            });
        }
    }

    // Phase 4 UE_LOG：每 10 秒輸出一次 GPU CA tick 總 ms（包含 pack + 所有 FlushRenderingCommands）
    const double TickEnd = FPlatformTime::Seconds();
    if (TickEnd - GpuTickLastLogTime >= 10.0)
    {
        GpuTickLastLogTime = TickEnd;
        const double TotalMs = (TickEnd - TickStart) * 1000.0;
        UE_LOG(LogTemp, Display,
            TEXT("[M-10 Phase 4] TickGpuZone total=%.2fms  mode=%s  (PIE console: stat quick for per-step breakdown)"),
            TotalMs, bAsync ? TEXT("async") : TEXT("sync"));
    }
}

void FTileWorld3D::DestroyTile(int32 x, int32 y, int32 z, EDestroyReason Reason)
{
    EMaterialType OldMat = GetTile(x, y, z);
    if (OldMat == EMaterialType::Air) return;
    SetTile(x, y, z, EMaterialType::Air);
    if (OnTileDestroyed) OnTileDestroyed(x, y, z, OldMat, Reason);
}

// ============================================================
// 座標轉換
// ============================================================

int32 FTileWorld3D::FloorDiv(int32 a, int32 b)
{
    return a / b - (a % b != 0 && (a ^ b) < 0 ? 1 : 0);
}

int32 FTileWorld3D::PosMod(int32 a, int32 b)
{
    return ((a % b) + b) % b;
}

FIntVector FTileWorld3D::WorldToChunk(int32 x, int32 y, int32 z)
{
    return FIntVector(FloorDiv(x, FChunk3D::Size),
                      FloorDiv(y, FChunk3D::Size),
                      FloorDiv(z, FChunk3D::Size));
}

FIntVector FTileWorld3D::WorldToLocal(int32 x, int32 y, int32 z)
{
    return FIntVector(PosMod(x, FChunk3D::Size),
                      PosMod(y, FChunk3D::Size),
                      PosMod(z, FChunk3D::Size));
}

// ============================================================
// 基礎 Tile 存取
// ============================================================

bool FTileWorld3D::InBounds(int32 x, int32 y, int32 z) const
{
    if (Width <= 0 || Height <= 0 || Depth <= 0) return true;
    return x >= 0 && y >= 0 && z >= 0 && x < Width && y < Height && z < Depth;
}

FTileCell FTileWorld3D::GetCell(int32 x, int32 y, int32 z) const
{
    FIntVector CC = WorldToChunk(x, y, z);
    FIntVector LC = WorldToLocal(x, y, z);
    FChunk3D* const* Found = Chunks.Find(CC);
    if (!Found) return FTileCell{};
    return (*Found)->CellAt(LC.X, LC.Y, LC.Z);
}

EMaterialType FTileWorld3D::GetTile(int32 x, int32 y, int32 z) const
{
    return static_cast<EMaterialType>(GetCell(x, y, z).MaterialID);
}

void FTileWorld3D::SetTile(int32 x, int32 y, int32 z, EMaterialType Mat, uint8 CA_StateInit)
{
    FIntVector CC = WorldToChunk(x, y, z);
    FIntVector LC = WorldToLocal(x, y, z);
    FChunk3D&  C  = *GetOrCreateChunk(CC);
    FTileCell& Cell = C.CellAt(LC.X, LC.Y, LC.Z);
    Cell.MaterialID = static_cast<uint8>(Mat);
    Cell.CA_State   = CA_StateInit;
    Cell.Flags      = 0;
    C.MarkDirty(LC.X, LC.Y, LC.Z);
    DirtyChunks.Add(CC);
    MarkNeighborsDirty(CC, LC.X, LC.Y, LC.Z);
}

void FTileWorld3D::SetFire(int32 x, int32 y, int32 z)
{
    if (GetCell(x, y, z).MaterialID != 0) return;  // 只在 Air 放置
    SetTile(x, y, z, EMaterialType::Fire, static_cast<uint8>(Rng.RandRange(30, 90)));
}

// ============================================================
// 內部寫入（CA 模擬用，不建立新 chunk）
// ============================================================

void FTileWorld3D::WriteCell(int32 x, int32 y, int32 z, const FTileCell& Cell)
{
    FIntVector CC = WorldToChunk(x, y, z);
    FIntVector LC = WorldToLocal(x, y, z);
    FChunk3D** Found = Chunks.Find(CC);
    if (!Found) return;
    FChunk3D* C = *Found;
    C->CellAt(LC.X, LC.Y, LC.Z) = Cell;
    C->MarkDirty(LC.X, LC.Y, LC.Z);
    DirtyChunks.Add(CC);
    MarkNeighborsDirty(CC, LC.X, LC.Y, LC.Z);
}

bool FTileWorld3D::IsUpdated(int32 x, int32 y, int32 z) const
{
    FIntVector CC = WorldToChunk(x, y, z);
    FIntVector LC = WorldToLocal(x, y, z);
    FChunk3D* const* Found = Chunks.Find(CC);
    if (!Found) return true;
    return (*Found)->IsUpdated(LC.X, LC.Y, LC.Z);
}

void FTileWorld3D::MarkUpdated(int32 x, int32 y, int32 z)
{
    FIntVector CC = WorldToChunk(x, y, z);
    FIntVector LC = WorldToLocal(x, y, z);
    FChunk3D** Found = Chunks.Find(CC);
    if (Found) (*Found)->MarkUpdated(LC.X, LC.Y, LC.Z);
}

bool FTileWorld3D::IsOccupied(int32 x, int32 y, int32 z) const
{
    return OccupiedCells.Contains(FIntVector(x, y, z));
}

void FTileWorld3D::MarkNeighborsDirty(FIntVector CC, int32 lx, int32 ly, int32 lz)
{
    constexpr int32 S = FChunk3D::Size;
    if (lx == 0)   PendingNeighborDirty.Add(CC + FIntVector(-1, 0, 0));
    if (lx == S-1) PendingNeighborDirty.Add(CC + FIntVector( 1, 0, 0));
    if (ly == 0)   PendingNeighborDirty.Add(CC + FIntVector( 0,-1, 0));
    if (ly == S-1) PendingNeighborDirty.Add(CC + FIntVector( 0, 1, 0));
    if (lz == 0)   PendingNeighborDirty.Add(CC + FIntVector( 0, 0,-1));
    if (lz == S-1) PendingNeighborDirty.Add(CC + FIntVector( 0, 0, 1));
}

void FTileWorld3D::FlushNeighborDirty()
{
    for (const FIntVector& CC : PendingNeighborDirty)
        DirtyChunks.Add(CC);
    PendingNeighborDirty.Empty();
}

void FTileWorld3D::MarkNeighborsCaDirty(int32 x, int32 y, int32 z)
{
    FIntVector CC = WorldToChunk(x, y, z);
    FIntVector LC = WorldToLocal(x, y, z);
    MarkNeighborsDirty(CC, LC.X, LC.Y, LC.Z);
}

int32 FTileWorld3D::HeightEstimator(int32 x, int32 z) const
{
    // 從頂部(y=0)往下掃，返回第一個非 Air 格的 Y（即「地表」）
    for (int32 y = 0; y < Height; ++y)
    {
        if (InBounds(x, y, z) && GetTile(x, y, z) != EMaterialType::Air)
            return y;
    }
    return Height > 0 ? Height - 1 : 0;
}

// ============================================================
// 實體佔用
// ============================================================

void FTileWorld3D::ClearOccupied()  { OccupiedCells.Empty(); }
void FTileWorld3D::SetOccupied(int32 x, int32 y, int32 z)
{
    OccupiedCells.Add(FIntVector(x, y, z));
}

// ============================================================
// CA Tick
// ============================================================

void FTileWorld3D::Tick(int32 ActiveCX, int32 ActiveCY, int32 ActiveCZ, int32 SimRadius)
{
    ++Frame;

    // M-10 Phase 3：GPU CA zone 跟著玩家所在 chunk 移動（ActiveCX/Y/Z 就是呼叫端算好的玩家
    // chunk 座標，見 AVoxelWorldActor.cpp 的 PCX/PCY/PCZ），取 chunk 中心當 zone 中心。
    // 跟 CPU 主迴圈不一樣，GPU zone 涵蓋範圍跟 chunk dirty 篩選無關，每幀固定做一次。
    if (GpuSim.IsAvailable())
    {
        const int32 PlayerWX = ActiveCX * FChunk3D::Size + FChunk3D::Size / 2;
        const int32 PlayerWY = ActiveCY * FChunk3D::Size + FChunk3D::Size / 2;
        const int32 PlayerWZ = ActiveCZ * FChunk3D::Size + FChunk3D::Size / 2;
        UpdateGpuOrigin(PlayerWX, PlayerWY, PlayerWZ);
        TickGpuZone();
    }

    // 快照待模擬 chunk（從 DirtyChunks 篩選半徑內）
    TArray<FIntVector> ToProcess;
    ToProcess.Reserve(DirtyChunks.Num());
    for (const FIntVector& CC : DirtyChunks)
    {
        if (SimRadius <= 0 ||
            (FMath::Abs(CC.X - ActiveCX) <= SimRadius &&
             FMath::Abs(CC.Y - ActiveCY) <= SimRadius &&
             FMath::Abs(CC.Z - ActiveCZ) <= SimRadius))
        {
            ToProcess.Add(CC);
        }
    }
    // 2026-06-19 稽核發現：先前這裡無條件 DirtyChunks.Empty()，半徑外被篩掉的 chunk
    // 會直接永久遺失（玩家走遠時變 dirty 的格子，回來後也不會再模擬，除非被其他
    // SetTile/WriteCell 呼叫重新標記）。對應 Godot TileWorld3D.cs:156-165 會把超出
    // 半徑的 dirty chunk 留到下一幀——這裡改成只移除這次真的要處理的 chunk，其餘留著。
    for (const FIntVector& CC : ToProcess)
        DirtyChunks.Remove(CC);

    // 排序：ChunkY 大的（底部）先處理 → 砂礫正確落下
    ToProcess.Sort([](const FIntVector& A, const FIntVector& B) { return A.Y > B.Y; });

    // 清除本幀 Updated 旗標
    for (const FIntVector& CC : ToProcess)
    {
        if (FChunk3D** Found = Chunks.Find(CC))
            (*Found)->ClearUpdated();
    }

    // CA 模擬主循環
    for (const FIntVector& CC : ToProcess)
    {
        FChunk3D** Found = Chunks.Find(CC);
        if (!Found) continue;
        FChunk3D* C = *Found;
        if (!C->IsDirty()) continue;

        // 記錄 dirty AABB 後清除（本幀新寫入的 cell 會重新展開 AABB）。
        // ±1 margin：CA 粒子會影響鄰格（對應 Godot TileWorld3D.cs:168-176），
        // 沒有這個 margin 會讓 dirty AABB 邊緣外一格的傳播延遲一幀才生效。
        int32 MinX = FMath::Max(0, C->DirtyMin[0] - 1), MaxX = FMath::Min(FChunk3D::Size - 1, C->DirtyMax[0] + 1);
        int32 MinY = FMath::Max(0, C->DirtyMin[1] - 1), MaxY = FMath::Min(FChunk3D::Size - 1, C->DirtyMax[1] + 1);
        int32 MinZ = FMath::Max(0, C->DirtyMin[2] - 1), MaxZ = FMath::Min(FChunk3D::Size - 1, C->DirtyMax[2] + 1);
        C->ClearDirty();

        for (int32 lz = MinZ; lz <= MaxZ; ++lz)
        for (int32 ly = MinY; ly <= MaxY; ++ly)
        for (int32 lx = MinX; lx <= MaxX; ++lx)
        {
            if (C->IsUpdated(lx, ly, lz)) continue;
            const FTileCell& Cell = C->CellAt(lx, ly, lz);
            if (Cell.MaterialID == 0) continue;  // Air

            int32 wx = CC.X * FChunk3D::Size + lx;
            int32 wy = CC.Y * FChunk3D::Size + ly;
            int32 wz = CC.Z * FChunk3D::Size + lz;

            // GPU zone 內的 Powder/Liquid 移動已由 TickGpuZone() 算過，CPU 不重算移動。
            // 但 Lava 的點火（TryIgniteAround）與元素 CA 反應（CheckElementalCaReactions）
            // 是化學反應，GPU shader 不負責，GPU zone 內仍要跑 CPU 版（Godot TileWorld3D.cs:287-288）。
            switch (FMaterialRegistry::GetPhysics(Cell.MaterialID))
            {
                case EPhysicsCategory::Powder:
                    if (!InGpuZone(wx, wy, wz)) UpdatePowder(wx, wy, wz);
                    break;
                case EPhysicsCategory::Liquid:
                    if (!InGpuZone(wx, wy, wz))
                    {
                        UpdateLiquid(wx, wy, wz);
                    }
                    else if (static_cast<EMaterialType>(Cell.MaterialID) == EMaterialType::Lava
                             && Frame % 3 == 0)
                    {
                        // GPU zone 內 Lava：僅執行點火 + 元素反應（移動由 GPU 負責）
                        TryIgniteAround(wx, wy, wz, 0.1f);
                        CheckElementalCaReactions(wx, wy, wz);
                    }
                    break;
                case EPhysicsCategory::Gas:    UpdateGas(wx, wy, wz);    break;
                case EPhysicsCategory::Static: UpdateStatic(wx, wy, wz); break;
                default: break;
            }
        }
    }

    // 套用 CA 元素反應（延遲寫入，避免 sweep 中途修改鄰格）
    for (const FCaReactionWrite& W : CaReactionQueue)
        SetTile(W.X, W.Y, W.Z, W.Mat);
    CaReactionQueue.Empty();

    // 跨 chunk 髒通知延遲刷新
    FlushNeighborDirty();
}

// ============================================================
// TryMove（CA 移動核心）
// ============================================================

bool FTileWorld3D::TryMove(int32 fx, int32 fy, int32 fz, int32 tx, int32 ty, int32 tz)
{
    // 垂直軸永遠需要邊界保護（Width=0 無限世界時跳過水平 InBounds，但 Y 仍有上限）
    if (ty < 0 || (Height > 0 && ty >= Height)) return false;
    if (Width > 0 && !InBounds(tx, ty, tz)) return false;

    // CA 不主動建立新 chunk（目標 chunk 不存在 = 牆壁）
    FIntVector ToCC = WorldToChunk(tx, ty, tz);
    if (!Chunks.Contains(ToCC)) return false;

    FTileCell FromCell = GetCell(fx, fy, fz);
    FTileCell ToCell   = GetCell(tx, ty, tz);

    const FMaterialData& FromData = FMaterialRegistry::Get(static_cast<EMaterialType>(FromCell.MaterialID));
    const FMaterialData& ToData   = FMaterialRegistry::Get(static_cast<EMaterialType>(ToCell.MaterialID));

    bool bPassable = false;
    if (ToCell.MaterialID == 0)  // Air
        bPassable = true;
    else if (ToData.Physics == EPhysicsCategory::Liquid)
        // Godot TileWorld3D.cs:357-359：目標是 Liquid 且移入方比目標重 → 可取代（含 Powder 沉水）
        bPassable = (FromData.Density > ToData.Density);

    if (!bPassable || IsOccupied(tx, ty, tz) || IsUpdated(tx, ty, tz))
        return false;

    WriteCell(fx, fy, fz, ToCell);
    WriteCell(tx, ty, tz, FromCell);
    MarkUpdated(tx, ty, tz);
    return true;
}

// ============================================================
// 物理分類更新
// ============================================================

void FTileWorld3D::UpdatePowder(int32 x, int32 y, int32 z)
{
    if (TryMove(x, y, z, x, y+1, z)) return;  // 垂直落下（Y+ = 向下）

    // 對角落下（隨機起始方向防偏斜）
    static const int32 Dx[] = {-1,  1,  0,  0};
    static const int32 Dz[] = { 0,  0, -1,  1};
    int32 Start = Rng.RandHelper(4);
    for (int32 i = 0; i < 4; ++i)
    {
        int32 j = (Start + i) % 4;
        if (TryMove(x, y, z, x + Dx[j], y+1, z + Dz[j])) return;
    }
}

void FTileWorld3D::UpdateLiquid(int32 x, int32 y, int32 z)
{
    FTileCell Cell = GetCell(x, y, z);
    EMaterialType Mat = static_cast<EMaterialType>(Cell.MaterialID);

    // G-3a：Lava 限速 — 每 3 幀才更新一次，使岩漿流速約為水的 1/3（Godot TileWorld3D.cs:265）
    if (Mat == EMaterialType::Lava && Frame % 3 != 0) return;

    if (TryMove(x, y, z, x, y+1, z)) return;

    static const int32 Dx[] = {-1,  1,  0,  0};
    static const int32 Dz[] = { 0,  0, -1,  1};
    int32 Start = Rng.RandHelper(4);
    for (int32 i = 0; i < 4; ++i)
    {
        int32 j = (Start + i) % 4;
        if (TryMove(x, y, z, x + Dx[j], y+1, z + Dz[j])) return;
    }

    // 橫向擴散（Water=3格，Lava=1格；方向由幀號決定防止偏斜）
    int32 Spread = (Mat == EMaterialType::Water) ? 3 : 1;
    int32 HX     = (Frame % 2 == 0) ? 1 : -1;
    int32 HZ     = (Frame % 4 < 2)  ? 1 : -1;
    for (int32 s = 1; s <= Spread; ++s)
        if (!TryMove(x, y, z, x + HX*s, y, z)) break;
    for (int32 s = 1; s <= Spread; ++s)
        if (!TryMove(x, y, z, x, y, z + HZ*s)) break;

    if (Mat == EMaterialType::Lava)
        TryIgniteAround(x, y, z, 0.1f);  // G-3b：Godot TileWorld3D.cs:287 為 0.1f（原 0.005f 差 20 倍）

    CheckElementalCaReactions(x, y, z);
}

void FTileWorld3D::UpdateGas(int32 x, int32 y, int32 z)
{
    FTileCell Cell = GetCell(x, y, z);
    EMaterialType Mat = static_cast<EMaterialType>(Cell.MaterialID);

    // 水滅火
    if (Mat == EMaterialType::Fire && HasAdjacentMaterial(x, y, z, EMaterialType::Water))
    {
        ExtinguishFire(x, y, z);
        return;
    }

    // 倒計時（CA_State > 0 則遞減，到 0 後消散）
    if (Cell.CA_State > 0)
    {
        --Cell.CA_State;
        WriteCell(x, y, z, Cell);
    }

    if (Mat == EMaterialType::Fire)
        TryIgniteAround(x, y, z, 0.02f);

    // 上浮（Y- = 向上）
    if (TryMove(x, y, z, x, y-1, z)) return;

    static const int32 Dx[] = {-1,  1,  0,  0};
    static const int32 Dz[] = { 0,  0, -1,  1};
    int32 Start = Rng.RandHelper(4);
    for (int32 i = 0; i < 4; ++i)
    {
        int32 j = (Start + i) % 4;
        if (TryMove(x, y, z, x + Dx[j], y-1, z + Dz[j])) return;
    }

    // 倒計時歸零 → 消散（CA_State 在 WriteCell 後已是最新值）
    Cell = GetCell(x, y, z);  // 重讀（WriteCell 可能已更新）
    if (Cell.CA_State == 0)
    {
        if      (Mat == EMaterialType::Fire)  SetTile(x, y, z, EMaterialType::Ash);
        else if (Mat == EMaterialType::Steam) SetTile(x, y, z, EMaterialType::Air);
    }
}

void FTileWorld3D::UpdateStatic(int32 x, int32 y, int32 z)
{
    FTileCell Cell = GetCell(x, y, z);
    if (Cell.CA_State == 0) return;  // 未點燃

    const FMaterialData& Data = FMaterialRegistry::Get(static_cast<EMaterialType>(Cell.MaterialID));
    if (!Data.bFlammable) return;

    --Cell.CA_State;
    if (Cell.CA_State == 0)
    {
        SetTile(x, y, z, EMaterialType::Ash);
        return;
    }

    WriteCell(x, y, z, Cell);
    TryIgniteAround(x, y, z, 0.05f);

    // 在上方空氣格生成火焰（Y=0 是頂層天空，y-1 會越界）
    if (y > 0 && GetCell(x, y-1, z).MaterialID == 0)
        SetFire(x, y-1, z);
}

// ============================================================
// 火焰輔助
// ============================================================

void FTileWorld3D::IgniteMaterial(int32 x, int32 y, int32 z)
{
    FTileCell Cell = GetCell(x, y, z);
    const FMaterialData& Data = FMaterialRegistry::Get(static_cast<EMaterialType>(Cell.MaterialID));
    // P-1: AutoignitionTemp >= 0 的材質也可被引燃（即使 bFlammable=false，如 Grass）
    if ((!Data.bFlammable && Data.AutoignitionTemp < 0.f) || Cell.CA_State > 0) return;
    int32 Min = Data.bFlammable ? Data.BurnMin : 10;
    int32 Max = Data.bFlammable ? Data.BurnMax : 30;
    Cell.CA_State = static_cast<uint8>(Rng.RandRange(Min, Max));
    WriteCell(x, y, z, Cell);
}

void FTileWorld3D::ExtinguishFire(int32 x, int32 y, int32 z)
{
    SetTile(x, y, z, EMaterialType::Steam,
            static_cast<uint8>(Rng.RandRange(60, 120)));
}

void FTileWorld3D::TryIgniteAround(int32 x, int32 y, int32 z, float Chance)
{
    constexpr int32 Dx[] = { 1,-1, 0, 0, 0, 0};
    constexpr int32 Dy[] = { 0, 0, 1,-1, 0, 0};
    constexpr int32 Dz[] = { 0, 0, 0, 0, 1,-1};
    for (int32 i = 0; i < 6; ++i)
    {
        int32 NX = x+Dx[i], NY = y+Dy[i], NZ = z+Dz[i];
        // P-1: AutoignitionTemp — 低自燃溫度材質用更高機率被引燃（Grass 180°C >> 3×Chance）
        float AdjChance = Chance;
        if (InBounds(NX, NY, NZ))
        {
            const FMaterialData& ND = FMaterialRegistry::Get(GetTile(NX, NY, NZ));
            if (!ND.bFlammable && ND.AutoignitionTemp >= 0.f)
                AdjChance = Chance * FMath::Max(1.f, 300.f / FMath::Max(1.f, ND.AutoignitionTemp));
        }
        if (Rng.FRand() < AdjChance)
            IgniteMaterial(NX, NY, NZ);
    }
}

bool FTileWorld3D::HasAdjacentMaterial(int32 x, int32 y, int32 z, EMaterialType Mat) const
{
    constexpr int32 Dx[] = { 1,-1, 0, 0, 0, 0};
    constexpr int32 Dy[] = { 0, 0, 1,-1, 0, 0};
    constexpr int32 Dz[] = { 0, 0, 0, 0, 1,-1};
    for (int32 i = 0; i < 6; ++i)
        if (GetTile(x+Dx[i], y+Dy[i], z+Dz[i]) == Mat) return true;
    return false;
}

// ============================================================
// Gameplay 操作
// ============================================================

void FTileWorld3D::Explode(int32 cx, int32 cy, int32 cz, int32 Radius, float Chance)
{
    TMap<EMaterialType, int32> DestroyedByMat;

    for (int32 dy = -Radius; dy <= Radius; ++dy)
    for (int32 dz = -Radius; dz <= Radius; ++dz)
    for (int32 dx = -Radius; dx <= Radius; ++dx)
    {
        if (dx*dx + dy*dy + dz*dz > Radius*Radius) continue;
        if (Chance < 1.f && Rng.FRand() >= Chance) continue;
        int32 wx = cx+dx, wy = cy+dy, wz = cz+dz;
        EMaterialType Mat = GetTile(wx, wy, wz);
        if (Mat != EMaterialType::Air)
        {
            DestroyedByMat.FindOrAdd(Mat)++;
            DestroyTile(wx, wy, wz, EDestroyReason::Explosion);
            if (Rng.FRand() < 0.3f && wy > 0)
                SetFire(wx, wy-1, wz);
        }
    }

    if (OnExplodeComplete && DestroyedByMat.Num() > 0)
        OnExplodeComplete(FIntVector(cx, cy, cz), DestroyedByMat);
}

FRaycastResult3D FTileWorld3D::Raycast(FVector Start, FVector Dir, float MaxDist) const
{
    FRaycastResult3D Result;
    if (!Dir.Normalize(1e-6)) return Result;

    int32 x = FMath::FloorToInt(Start.X);
    int32 y = FMath::FloorToInt(Start.Y);
    int32 z = FMath::FloorToInt(Start.Z);

    int32 sx = Dir.X >= 0 ? 1 : -1;
    int32 sy = Dir.Y >= 0 ? 1 : -1;
    int32 sz = Dir.Z >= 0 ? 1 : -1;

    double tDX = Dir.X != 0.0 ? FMath::Abs(1.0 / Dir.X) : TNumericLimits<double>::Max();
    double tDY = Dir.Y != 0.0 ? FMath::Abs(1.0 / Dir.Y) : TNumericLimits<double>::Max();
    double tDZ = Dir.Z != 0.0 ? FMath::Abs(1.0 / Dir.Z) : TNumericLimits<double>::Max();

    double tMX = Dir.X != 0.0 ? (sx > 0 ? (double)(x+1)-Start.X : Start.X-(double)x) * tDX : TNumericLimits<double>::Max();
    double tMY = Dir.Y != 0.0 ? (sy > 0 ? (double)(y+1)-Start.Y : Start.Y-(double)y) * tDY : TNumericLimits<double>::Max();
    double tMZ = Dir.Z != 0.0 ? (sz > 0 ? (double)(z+1)-Start.Z : Start.Z-(double)z) * tDZ : TNumericLimits<double>::Max();

    double t = 0.0;
    FIntVector Normal(0, 0, 0);

    while (t <= (double)MaxDist)
    {
        if (GetTile(x, y, z) != EMaterialType::Air)
        {
            Result.bHit       = true;
            Result.HitCell    = FIntVector(x, y, z);
            Result.FaceNormal = Normal;
            Result.Distance   = (float)t;
            return Result;
        }
        if (tMX < tMY && tMX < tMZ)
        { t = tMX; x += sx; Normal = FIntVector(-sx, 0,   0  ); tMX += tDX; }
        else if (tMY < tMZ)
        { t = tMY; y += sy; Normal = FIntVector( 0, -sy,  0  ); tMY += tDY; }
        else
        { t = tMZ; z += sz; Normal = FIntVector( 0,  0,  -sz ); tMZ += tDZ; }
    }
    return Result;
}

TArray<FTileCell> FTileWorld3D::SnapshotRegion(FIntVector Min, FIntVector Max) const
{
    TArray<FTileCell> Result;
    int32 W = Max.X-Min.X+1, H = Max.Y-Min.Y+1, D = Max.Z-Min.Z+1;
    Result.Reserve(W*H*D);
    for (int32 z = Min.Z; z <= Max.Z; ++z)
    for (int32 y = Min.Y; y <= Max.Y; ++y)
    for (int32 x = Min.X; x <= Max.X; ++x)
        Result.Add(GetCell(x, y, z));
    return Result;
}

void FTileWorld3D::RestoreRegion(FIntVector Min, FIntVector Max, const TArray<FTileCell>& Snapshot)
{
    int32 Idx = 0;
    for (int32 z = Min.Z; z <= Max.Z; ++z)
    for (int32 y = Min.Y; y <= Max.Y; ++y)
    for (int32 x = Min.X; x <= Max.X; ++x)
        WriteCell(x, y, z, Snapshot[Idx++]);
}

// ============================================================
// 元素 CA 反應
// ============================================================

// 在 UpdateLiquid 末尾呼叫：掃描 6 鄰格，處理兩條硬碼 CA 反應
// ・Water NativeElement 鄰格 Dirt → Sand（0.5%）
// ・Lava + Water NativeElement 鄰格 → Stone + Steam（3%）
void FTileWorld3D::CheckElementalCaReactions(int32 x, int32 y, int32 z)
{
    EMaterialType Mat = GetTile(x, y, z);
    const FMaterialData& MatData = FMaterialRegistry::Get(Mat);
    if (MatData.NativeElement == ESkillElementType::None) return;

    constexpr int32 Dx[] = { 1,-1, 0, 0, 0, 0 };
    constexpr int32 Dy[] = { 0, 0, 1,-1, 0, 0 };
    constexpr int32 Dz[] = { 0, 0, 0, 0, 1,-1 };

    for (int32 i = 0; i < 6; ++i)
    {
        int32 nx = x + Dx[i], ny = y + Dy[i], nz = z + Dz[i];
        if (!InBounds(nx, ny, nz)) continue;
        EMaterialType NMat = GetTile(nx, ny, nz);
        const FMaterialData& NData = FMaterialRegistry::Get(NMat);

        // Water（NativeElement） + Dirt 鄰格 → Sand（慢速 0.5%）
        if (MatData.NativeElement == ESkillElementType::Water &&
            NMat == EMaterialType::Dirt_Dry &&
            Rng.GetFraction() < 0.005f)
        {
            // 延遲寫入：在 CA sweep 結束後才套用，避免中途修改鄰格（CA 一致性）
            CaReactionQueue.Add({nx, ny, nz, EMaterialType::Sand});
        }

        // Lava + Water（NativeElement）鄰格 → Stone + Steam（快速 3%）
        if (Mat == EMaterialType::Lava &&
            NData.NativeElement == ESkillElementType::Water &&
            Rng.GetFraction() < 0.03f)
        {
            CaReactionQueue.Add({x,  y,  z,  EMaterialType::Stone_Cobble});
            CaReactionQueue.Add({nx, ny, nz, EMaterialType::Steam});
            return;
        }
    }
}

// 法術命中 API：根據衝擊元素與格子 NativeElement 查反應表，套用 CA 效果
void FTileWorld3D::ApplyElementalImpact(int32 x, int32 y, int32 z, ESkillElementType ImpactElement)
{
    if (!InBounds(x, y, z) || ImpactElement == ESkillElementType::None) return;

    EMaterialType Mat = GetTile(x, y, z);
    const FMaterialData& TileData = FMaterialRegistry::Get(Mat);

    // P-3: FreezeToMaterial — Ice 元素命中時直接轉換（優先於反應表）
    if (ImpactElement == ESkillElementType::Ice
        && TileData.FreezeToMaterial != EMaterialType::Air)
    {
        SetTile(x, y, z, TileData.FreezeToMaterial);
        return;
    }

    // P-2: MeltToMaterial — Fire 元素命中時直接轉換（優先於反應表）
    if (ImpactElement == ESkillElementType::Fire
        && TileData.MeltToMaterial != EMaterialType::Air)
    {
        SetTile(x, y, z, TileData.MeltToMaterial);
        return;
    }

    // P-4: Thunder — 感電傳播 BFS（不中斷，繼續查反應表）
    if (ImpactElement == ESkillElementType::Thunder)
        PropagateThunder(x, y, z);

    ESkillElementType TileElem = TileData.NativeElement;
    if (TileElem == ESkillElementType::None) return;

    const FReactionDef* Reaction = FElementalReactionTable::Lookup(ImpactElement, TileElem);
    if (!Reaction) return;

    if (Reaction->Name == TEXT("沸騰"))                          // Fire+Water → Steam
        SetTile(x, y, z, EMaterialType::Steam);
    else if (Reaction->Name == TEXT("流沙") && Mat == EMaterialType::Dirt_Dry)  // Water+Earth → Sand
        SetTile(x, y, z, EMaterialType::Sand);
    else if (Reaction->Name == TEXT("燃燒"))                    // Fire+Wood → ignite
        IgniteMaterial(x, y, z);
    else if (Reaction->Name == TEXT("熔爐"))                    // Fire+Metal → Lava（W-3b）
        SetTile(x, y, z, EMaterialType::Lava);
}

// P-4: Thunder 感電傳播 BFS — 沿 ElectricalConductivity > Threshold 的鄰格擴散
void FTileWorld3D::PropagateThunder(int32 x, int32 y, int32 z, float Threshold, int32 MaxSteps)
{
    struct FThunderCell { int32 X, Y, Z; };
    TArray<FThunderCell> Queue;
    TSet<FIntVector> Visited;

    Queue.Add({x, y, z});
    Visited.Add({x, y, z});

    constexpr int32 Dx[] = { 1,-1, 0, 0, 0, 0};
    constexpr int32 Dy[] = { 0, 0, 1,-1, 0, 0};
    constexpr int32 Dz[] = { 0, 0, 0, 0, 1,-1};

    int32 Steps = 0;
    while (Queue.Num() > 0 && Steps < MaxSteps)
    {
        FThunderCell Cur = Queue[0];
        Queue.RemoveAt(0, 1, false);
        ++Steps;

        const FMaterialData& D = FMaterialRegistry::Get(GetTile(Cur.X, Cur.Y, Cur.Z));
        if (D.ElectricalConductivity < Threshold) continue;

        for (int32 i = 0; i < 6; ++i)
        {
            int32 NX = Cur.X + Dx[i], NY = Cur.Y + Dy[i], NZ = Cur.Z + Dz[i];
            FIntVector Key{NX, NY, NZ};
            if (!InBounds(NX, NY, NZ) || Visited.Contains(Key)) continue;
            Visited.Add(Key);
            Queue.Add({NX, NY, NZ});
        }
    }
}

// ============================================================
// Chunk 管理
// ============================================================

FChunk3D* FTileWorld3D::GetOrCreateChunk(FIntVector CC)
{
    if (FChunk3D** Found = Chunks.Find(CC))
        return *Found;
    FChunk3D* New = new FChunk3D(CC);
    Chunks.Add(CC, New);
    return New;
}

FChunk3D* FTileWorld3D::GetChunkAt(FIntVector CC) const
{
    FChunk3D* const* Found = Chunks.Find(CC);
    return Found ? *Found : nullptr;
}

void FTileWorld3D::SaveChunk(int32 cx, int32 cy, int32 cz, const FString& WorldDir)
{
    FIntVector CC(cx, cy, cz);
    FChunk3D** Found = Chunks.Find(CC);
    if (!Found || !(*Found)->bNeedsSave) return;

    FString Path = FString::Printf(TEXT("%s/chunks/%d_%d_%d.bin"), *WorldDir, cx, cy, cz);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);

    TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileWriter(*Path));
    if (!Ar) return;

    uint32 Magic = 0x434B3344u;  // 'CK3D'
    uint8  Ver   = 2;  // H-4：FTileCell 加 Variant 欄位（4→5 bytes），版號 bump 讓舊存檔安全重生成
    *Ar << Magic << Ver;
    Ar->Serialize((*Found)->Cells, sizeof((*Found)->Cells));
    (*Found)->bNeedsSave = false;
}

bool FTileWorld3D::TryLoadChunk(int32 cx, int32 cy, int32 cz, const FString& WorldDir)
{
    FString Path = FString::Printf(TEXT("%s/chunks/%d_%d_%d.bin"), *WorldDir, cx, cy, cz);
    TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileReader(*Path));
    if (!Ar) return false;

    uint32 Magic = 0; uint8 Ver = 0;
    *Ar << Magic << Ver;
    if (Magic != 0x434B3344u || Ver != 2) return false;  // Ver 1（4 bytes/cell）視為過期，重新生成

    FIntVector CC(cx, cy, cz);
    FChunk3D& C = *GetOrCreateChunk(CC);
    Ar->Serialize(C.Cells, sizeof(C.Cells));

    // 標記整個 chunk 為 dirty 供渲染初始化；載入自磁碟的 chunk 不需要重存
    C.DirtyMin[0] = C.DirtyMin[1] = C.DirtyMin[2] = 0;
    C.DirtyMax[0] = C.DirtyMax[1] = C.DirtyMax[2] = FChunk3D::Size - 1;
    C.bDirty = true;
    C.bMeshNeedsRebuild = true;
    C.bNeedsSave = false;
    DirtyChunks.Add(CC);
    return true;
}

FChunk3D* FTileWorld3D::EnsureChunkAt(int32 x, int32 y, int32 z)
{
    return GetOrCreateChunk(WorldToChunk(x, y, z));
}

void FTileWorld3D::SaveDirtyChunks(const FString& WorldDir)
{
    for (auto& Pair : Chunks)
        if (Pair.Value->bNeedsSave)
            SaveChunk(Pair.Key.X, Pair.Key.Y, Pair.Key.Z, WorldDir);
}

TArray<FIntVector> FTileWorld3D::EvictFarChunks(int32 cx, int32 cy, int32 cz,
                                                  int32 KeepRadius, const FString& WorldDir)
{
    TArray<FIntVector> Evicted;
    for (auto It = Chunks.CreateIterator(); It; ++It)
    {
        const FIntVector& CC = It.Key();
        bool bFar = (FMath::Abs(CC.X - cx) > KeepRadius ||
                     FMath::Abs(CC.Y - cy) > KeepRadius ||
                     FMath::Abs(CC.Z - cz) > KeepRadius);
        if (bFar)
        {
            if (It.Value()->bNeedsSave)
                SaveChunk(CC.X, CC.Y, CC.Z, WorldDir);
            FChunk3D* ToDelete = It.Value();
            Evicted.Add(CC);
            It.RemoveCurrent();
            delete ToDelete;
        }
    }
    return Evicted;
}
