#include "UDecorationSubsystem.h"
#include "AVoxelWorldActor.h"
#include "TileWorld3D.h"
#include "WorldScale.h"
#include "MaterialType.h"
#include "Engine/StaticMesh.h"

// ============================================================
// 生命週期
// ============================================================

void UDecorationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FDecorationRegistry::Initialize();
}

void UDecorationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    AVoxelWorldActor* VWA = AVoxelWorldActor::FindInWorld(&InWorld);
    if (!VWA) return;

    CachedVWA = VWA;

    ChunkAppliedHandle = VWA->OnChunkApplied.AddUObject(
        this, &UDecorationSubsystem::OnChunkApplied);
    ChunkEvictedHandle = VWA->OnChunkEvicted.AddUObject(
        this, &UDecorationSubsystem::OnChunkEvicted);
}

void UDecorationSubsystem::Deinitialize()
{
    if (CachedVWA.IsValid())
    {
        CachedVWA->OnChunkApplied.Remove(ChunkAppliedHandle);
        CachedVWA->OnChunkEvicted.Remove(ChunkEvictedHandle);
    }
    Super::Deinitialize();
}

// ============================================================
// DECO-3：chunk 生成 → 地表掃描 + 放置裝飾
// ============================================================

void UDecorationSubsystem::OnChunkApplied(FIntVector CC)
{
    // 每個 chunk 只處理一次（eviction 後若重新 applied 才重新放置）
    if (ChunkTransforms.Contains(CC)) return;

    if (!CachedVWA.IsValid()) return;
    AVoxelWorldActor* VWA = CachedVWA.Get();
    FTileWorld3D* TW = VWA->GetTileWorld();
    const int32 WorldH = VWA->WorldHeight;

    constexpr int32 S = WorldScale::ChunkSize;
    const int32 ChunkYMin = CC.Y * S;
    const int32 ChunkYMax = (CC.Y + 1) * S - 1;

    const TArray<FDecorationDef>& AllDefs = FDecorationRegistry::GetAll();
    if (AllDefs.IsEmpty()) return;

    // ── DECO-4：預計算 chunk 內地表 Y（避免 HeightEstimator 重複呼叫）────────
    // SurfaceGrid[lz][lx] = 地表 voxel Y；-1 表示不在此 chunk 的 Y 範圍內
    TArray<int32> SurfaceGrid;
    SurfaceGrid.SetNumUninitialized(S * S);

    for (int32 lz = 0; lz < S; ++lz)
    for (int32 lx = 0; lx < S; ++lx)
    {
        const int32 wx = CC.X * S + lx;
        const int32 wz = CC.Z * S + lz;
        const int32 SY = TW->HeightEstimator(wx, wz);
        // 只有地表在本 chunk Y 範圍內才有意義
        SurfaceGrid[lz * S + lx] = (SY >= ChunkYMin && SY <= ChunkYMax) ? SY : -1;
    }

    TArray<TPair<FName, FTransform>>& ChunkDecos = ChunkTransforms.FindOrAdd(CC);

    for (int32 lz = 0; lz < S; ++lz)
    for (int32 lx = 0; lx < S; ++lx)
    {
        const int32 SurfaceY = SurfaceGrid[lz * S + lx];
        if (SurfaceY < 0) continue;  // 地表不在此 chunk

        const int32 wx = CC.X * S + lx;
        const int32 wz = CC.Z * S + lz;
        const EMaterialType SurfaceMat = TW->GetTile(wx, SurfaceY, wz);
        // 排除 Air / Water（HeightEstimator 在未生成 chunk 返回 Air）
        if (SurfaceMat == EMaterialType::Air || SurfaceMat == EMaterialType::Water) continue;

        // ── 坡度計算（利用已計算的 SurfaceGrid，僅在 chunk 內部做比較）─────
        auto GetNeighborSY = [&](int32 dx, int32 dz) -> int32 {
            const int32 nx = lx + dx, nz = lz + dz;
            if (nx < 0 || nx >= S || nz < 0 || nz >= S) return SurfaceY; // 邊界：假設同高
            const int32 Neighbor = SurfaceGrid[nz * S + nx];
            return (Neighbor >= 0) ? Neighbor : SurfaceY;
        };
        const float DH0 = FMath::Abs(float(GetNeighborSY( 1, 0) - SurfaceY));
        const float DH1 = FMath::Abs(float(GetNeighborSY(-1, 0) - SurfaceY));
        const float DH2 = FMath::Abs(float(GetNeighborSY( 0, 1) - SurfaceY));
        const float DH3 = FMath::Abs(float(GetNeighborSY( 0,-1) - SurfaceY));
        const float MaxDH = FMath::Max(FMath::Max(DH0, DH1), FMath::Max(DH2, DH3));
        // slope = atan(dH tile / 1 tile)（tile 間水平距離 = 1 tile）
        const float SlopeDeg = FMath::RadiansToDegrees(FMath::Atan(MaxDH));

        for (const FDecorationDef& Def : AllDefs)
        {
            if (!Def.ValidSurfaces.Contains(SurfaceMat)) continue;
            if (SlopeDeg > Def.MaxSlopeDeg) continue;

            float Scale = 1.f;
            if (!ShouldPlace(CC, lx, lz, Def, Scale)) continue;  // DECO-4

            // 裝飾放置在地表 tile 中心上方半格（地表頂面）
            FVector WorldPos = WorldScale::TileToWorld(wx, SurfaceY, wz, WorldH);
            WorldPos.Z += WorldScale::TileSizeCm * 0.5f;  // tile 中心 → 頂面

            // 確定性 Yaw 旋轉（避免整齊排列感）
            uint32 RotHash = (uint32)(wx * 1664525u ^ wz * 22695477u ^ (uint32)GetTypeHash(Def.PropId));
            RotHash = RotHash * 1664525u + 1013904223u;
            const float Yaw = float(RotHash & 0xFFFF) / 65535.f * 360.f;

            FTransform T(FRotator(0.f, Yaw, 0.f), WorldPos, FVector(Scale));
            ChunkDecos.Emplace(Def.PropId, T);

            if (UInstancedStaticMeshComponent* ISMC = GetOrCreateISMC(Def))
                ISMC->AddInstance(T);
        }
    }
}

// ============================================================
// DECO-5：chunk 淘汰 → 移除對應裝飾並全量重建 ISMC
// ============================================================

void UDecorationSubsystem::OnChunkEvicted(FIntVector CC)
{
    if (!ChunkTransforms.Contains(CC)) return;
    ChunkTransforms.Remove(CC);
    RebuildAllISMCs();
}

// ============================================================
// 內部輔助
// ============================================================

UInstancedStaticMeshComponent* UDecorationSubsystem::GetOrCreateISMC(const FDecorationDef& Def)
{
    if (TObjectPtr<UInstancedStaticMeshComponent>* Existing = ISMCByProp.Find(Def.PropId))
    {
        if (IsValid(*Existing)) return *Existing;
        ISMCByProp.Remove(Def.PropId);  // 無效引用，重新建立
    }

    if (!CachedVWA.IsValid()) return nullptr;

    UStaticMesh* Mesh = Cast<UStaticMesh>(Def.MeshPath.TryLoad());
    if (!Mesh)
    {
        // 資產尚未匯入：靜默跳過，不影響其他裝飾
        return nullptr;
    }

    AVoxelWorldActor* VWA = CachedVWA.Get();

    UInstancedStaticMeshComponent* ISMC = NewObject<UInstancedStaticMeshComponent>(VWA);
    ISMC->SetStaticMesh(Mesh);
    ISMC->SetMobility(EComponentMobility::Movable);  // 允許動態增刪 instance
    ISMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    ISMC->SetCollisionResponseToAllChannels(ECR_Block);
    ISMC->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    ISMC->SetCastShadow(false);  // 初版關閉陰影避免效能問題
    ISMC->RegisterComponent();
    ISMC->AttachToComponent(VWA->GetRootComponent(),
                            FAttachmentTransformRules::KeepWorldTransform);
    VWA->AddInstanceComponent(ISMC);

    ISMCByProp.Add(Def.PropId, ISMC);
    return ISMC;
}

// DECO-5：全量重建（ClearInstances 後依 ChunkTransforms 逐一 AddInstance）
// 效能：每次 eviction 觸發一次，eviction 頻率低（每 128 幀一次，且只在玩家離遠時），可接受
void UDecorationSubsystem::RebuildAllISMCs()
{
    // 清空所有 ISMC
    for (auto& [PropId, ISMC] : ISMCByProp)
        if (IsValid(ISMC)) ISMC->ClearInstances();

    // 依剩餘 ChunkTransforms 重新填入
    for (const auto& [CC, DecoList] : ChunkTransforms)
    {
        for (const auto& [PropId, T] : DecoList)
        {
            TObjectPtr<UInstancedStaticMeshComponent>* ISMCPtr = ISMCByProp.Find(PropId);
            if (ISMCPtr && IsValid(*ISMCPtr))
                (*ISMCPtr)->AddInstance(T);
        }
    }
}

// ============================================================
// DECO-4：確定性放置決策（純 hash，thread-safe，無 RNG 狀態）
// ============================================================

bool UDecorationSubsystem::ShouldPlace(FIntVector CC, int32 LX, int32 LZ,
                                        const FDecorationDef& Def, float& OutScale) const
{
    uint32 H = (uint32)(CC.X * 1664525u)
             ^ (uint32)(CC.Z * 22695477u)
             ^ (uint32)(LX  * 1013904223u)
             ^ (uint32)(LZ  * 2654435761u)
             ^ (uint32)GetTypeHash(Def.PropId);
    H = H * 1664525u + 1013904223u;

    const float F = float(H & 0xFFFFu) / 65535.f;  // 0..1
    if (F >= Def.Density) return false;

    // Scale 也由 hash 確定性決定
    H = H * 1664525u + 1013904223u;
    OutScale = FMath::Lerp(Def.ScaleRange.X, Def.ScaleRange.Y,
                           float(H & 0xFFFFu) / 65535.f);
    return true;
}
