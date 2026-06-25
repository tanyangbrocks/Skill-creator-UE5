#pragma once
#include "CoreMinimal.h"
#include "Chunk3D.h"
#include "MaterialRegistry.h"
#include "CaGpuSimulator.h"
#include "WorldTypes.h"

struct FRaycastResult3D
{
    FIntVector HitCell    = FIntVector(0, 0, 0);
    FIntVector FaceNormal = FIntVector(0, 0, 0);
    float      Distance   = 0.f;
    bool       bHit       = false;
};

// 純 C++ CA 世界容器（不繼承 UObject，背景 task 傳值安全）
// 座標約定：Y=0 為世界頂部，Y 增大方向為重力方向（向下）
// M-6 渲染：掃描 GetActiveChunks()，處理 bMeshNeedsRebuild
class VOXELWORLD_API FTileWorld3D
{
public:
    FTileWorld3D();
    ~FTileWorld3D();

    // 世界尺寸（0 = 無限懶載入 chunk）
    int32 Width  = 0;
    int32 Height = 0;
    int32 Depth  = 0;
    int32 WorldSeed = 12345;

    // --- 實體佔用（CA 不能移入已佔用格） ---
    void ClearOccupied();
    void SetOccupied(int32 x, int32 y, int32 z);

    // --- Tile 存取 ---
    bool          InBounds(int32 x, int32 y, int32 z) const;
    FTileCell     GetCell(int32 x, int32 y, int32 z) const;
    EMaterialType GetTile(int32 x, int32 y, int32 z) const;
    void          SetTile(int32 x, int32 y, int32 z, EMaterialType Mat, uint8 CA_StateInit = 0);
    void          SetFire(int32 x, int32 y, int32 z);  // 僅在 Air 上放置 Fire + 隨機 CA_State

    // --- CA 模擬（每幀呼叫） ---
    // SimRadius=-1 → 模擬全部 dirty chunk
    void Tick(int32 ActiveCX = 0, int32 ActiveCY = 0, int32 ActiveCZ = 0, int32 SimRadius = -1);

    // --- GPU CA（M-10）---
    // 初始化 GPU 模擬器；在世界建立後呼叫一次
    void InitGpu();
    // 移動 GPU 模擬區中心到玩家所在格（每幀或玩家跨 chunk 時呼叫）
    void UpdateGpuOrigin(int32 wx, int32 wy, int32 wz);
    // 是否在目前 GPU 模擬區範圍內
    bool InGpuZone(int32 x, int32 y, int32 z) const;
    // 將 GPU readback 的一格寫入世界（由 FCaGpuSimulator::Download 回呼）。
    // PackedCell 是 CaCellPacking::Pack() 格式（不是純 MaterialID byte——Phase 2 改了
    // FCaGpuSimulator::Download 的回呼簽名，這裡跟著改，原本的 uint8 版本不夠用）。
    void SetCellFromGpu(int32 x, int32 y, int32 z, uint32 PackedCell);

    // --- Gameplay ---
    // 摧毀格（設為 Air），並觸發 OnTileDestroyed 回呼（掉落物 / 特效使用）
    void             DestroyTile(int32 x, int32 y, int32 z, EDestroyReason Reason = EDestroyReason::Mining);
    void             Explode(int32 cx, int32 cy, int32 cz, int32 Radius, float Chance = 1.f);

    // tile 摧毀事件（由 AVoxelWorldActor / SkillCreatorRuntime 從外部綁定）
    TFunction<void(int32 x, int32 y, int32 z, EMaterialType OldMat, EDestroyReason Reason)> OnTileDestroyed;
    // 爆炸聚合事件：Explode() 結束後一次回報各材質摧毀 tile 總數（docs/plan-debris-fragment.md §D-2）
    TFunction<void(FIntVector Center, const TMap<EMaterialType, int32>& DestroyedByMat)> OnExplodeComplete;
    FRaycastResult3D Raycast(FVector Start, FVector Dir, float MaxDist) const;
    TArray<FTileCell> SnapshotRegion(FIntVector Min, FIntVector Max) const;
    void              RestoreRegion(FIntVector Min, FIntVector Max, const TArray<FTileCell>& Snapshot);
    // 法術命中後元素 CA 效果：沸騰→Steam / 流沙→Sand / 燃燒→IgniteMaterial（M-5）
    void              ApplyElementalImpact(int32 x, int32 y, int32 z, ESkillElementType ImpactElement);

    // --- 高度估算（從 y=0 往下找第一個非 Air 格的 Y 值；若全為 Air 返回 Height-1）---
    int32 HeightEstimator(int32 x, int32 z) const;

    // --- 確保 (x,y,z) 所在 chunk 存在記憶體中（若未載入則建立空 chunk，防止 GetCell 誤回傳 Air）---
    FChunk3D* EnsureChunkAt(int32 x, int32 y, int32 z);

    // --- CA 鄰居骯髒標記（公開版：由世界座標換算 chunk 後呼叫 MarkNeighborsDirty）---
    void MarkNeighborsCaDirty(int32 x, int32 y, int32 z);

    // 將 PendingNeighborDirty 批次沖洗至 DirtyChunks（Tick 末尾自動呼叫，外部亦可手動呼叫）
    void FlushNeighborDirty();

    // --- Chunk 管理 ---
    FChunk3D*  GetOrCreateChunk(FIntVector ChunkCoord);
    FChunk3D*  GetChunkAt(FIntVector ChunkCoord) const;
    bool       TryLoadChunk(int32 cx, int32 cy, int32 cz, const FString& WorldDir);
    void       SaveChunk(int32 cx, int32 cy, int32 cz, const FString& WorldDir);
    void       SaveDirtyChunks(const FString& WorldDir);
    TArray<FIntVector> EvictFarChunks(int32 cx, int32 cy, int32 cz, int32 KeepRadius, const FString& WorldDir);

    // 2026-06-23：切換到不同世界時呼叫（AVoxelWorldActor::ReinitializeForWorld）——刪除
    // 全部已載入 chunk（in-memory），不寫回磁碟（呼叫端決定要不要先存舊世界）。沿用
    // 解構子同款清理邏輯，供 destructor 跟這個方法共用。
    void ClearAllChunks();

    // M-6 渲染器唯讀存取
    const TMap<FIntVector, FChunk3D*>& GetActiveChunks() const { return Chunks; }

private:
    FCaGpuSimulator             GpuSim;

    struct FCaReactionWrite { int32 X, Y, Z; EMaterialType Mat; };

    TMap<FIntVector, FChunk3D*> Chunks;
    TSet<FIntVector>                        DirtyChunks;
    TSet<FIntVector>                        PendingNeighborDirty;
    TSet<FIntVector>                        OccupiedCells;
    TArray<FCaReactionWrite>                CaReactionQueue;
    FRandomStream                            Rng;
    int32                                   Frame = 0;

    // --- 座標轉換 ---
    static int32      FloorDiv(int32 a, int32 b);
    static int32      PosMod(int32 a, int32 b);
    static FIntVector WorldToChunk(int32 x, int32 y, int32 z);
    static FIntVector WorldToLocal(int32 x, int32 y, int32 z);

    // --- 內部存取 ---
    void WriteCell(int32 x, int32 y, int32 z, const FTileCell& Cell);
    bool IsUpdated(int32 x, int32 y, int32 z) const;
    void MarkUpdated(int32 x, int32 y, int32 z);
    bool IsOccupied(int32 x, int32 y, int32 z) const;
    void MarkNeighborsDirty(FIntVector CC, int32 lx, int32 ly, int32 lz);

    // --- GPU CA（M-10 Phase 3）：每幀對目前 GPU zone 做一次完整 Upload→Simulate→Download ---
    void TickGpuZone();

    // --- CA 物理 ---
    bool TryMove(int32 fx, int32 fy, int32 fz, int32 tx, int32 ty, int32 tz);
    void UpdatePowder(int32 x, int32 y, int32 z);
    void UpdateLiquid(int32 x, int32 y, int32 z);
    void UpdateGas(int32 x, int32 y, int32 z);
    void UpdateStatic(int32 x, int32 y, int32 z);
    void CheckElementalCaReactions(int32 x, int32 y, int32 z); // 每幀 UpdateLiquid 末尾呼叫

    // --- 火焰輔助 ---
    void IgniteMaterial(int32 x, int32 y, int32 z);
    void ExtinguishFire(int32 x, int32 y, int32 z);
    void TryIgniteAround(int32 x, int32 y, int32 z, float Chance);
    bool HasAdjacentMaterial(int32 x, int32 y, int32 z, EMaterialType Mat) const;

    // --- 元素傳播（Phase 2） ---
    // P-4：Thunder 感電傳播，BFS 沿 ElectricalConductivity > Threshold 的格子擴散
    void PropagateThunder(int32 x, int32 y, int32 z, float Threshold = 0.3f, int32 MaxSteps = 20);
};
