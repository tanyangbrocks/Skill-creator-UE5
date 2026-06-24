#include "AVoxelWorldActor.h"
#include "GreedyMesher.h"
#include "WorldScale.h"
#include "Chunk3D.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TileMaterialRegistry.h"
#include "MaterialType.h"
#include "DrawDebugHelpers.h"

// ============================================================
// 構造
// ============================================================

AVoxelWorldActor::AVoxelWorldActor()
{
    PrimaryActorTick.bCanEverTick = true;

    RMCComp = CreateDefaultSubobject<URealtimeMeshComponent>(TEXT("RMC"));
    SetRootComponent(RMCComp);

    // 自動載入 M_Voxel 材質（fallback）
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
        TEXT("/Game/M_Voxel.M_Voxel"));
    if (MatFinder.Succeeded())
        VoxelMaterial = MatFinder.Object;

    // AR-B：自動載入 Tile 材質登記表（不存在時保持 null，BeginPlay 會 fallback 到 VoxelMaterial）
    static ConstructorHelpers::FObjectFinder<UTileMaterialRegistry> RegFinder(
        TEXT("/Game/Data/DA_TileMaterialRegistry.DA_TileMaterialRegistry"));
    if (RegFinder.Succeeded())
        TileMaterialRegistry = RegFinder.Object;

    // 採掘高亮：引擎內建 Cube mesh + 半透明材質（預設隱藏）
    HighlightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HighlightMesh"));
    HighlightMesh->SetupAttachment(RMCComp);
    HighlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HighlightMesh->SetVisibility(false);
    HighlightMesh->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeFinder.Succeeded())
        HighlightMesh->SetStaticMesh(CubeFinder.Object);
}

// ============================================================
// Utilities
// ============================================================

AVoxelWorldActor* AVoxelWorldActor::FindInWorld(UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AVoxelWorldActor> It(World); It; ++It)
        return *It;
    return nullptr;
}

void AVoxelWorldActor::ShowHighlight(const TArray<FGridPos>& Tiles)
{
    if (Tiles.Num() == 0) { HideHighlight(); return; }
    if (!HighlightMesh) return;

    // Tiles[0]＝準心瞄準的中心格：沿用原本單格 Cube mesh + 青色線框
    const FVector Center = WorldScale::TileToWorld(Tiles[0], TileWorld.Height);
    const float S = WorldScale::TileSizeCm / 100.f;
    HighlightMesh->SetWorldScale3D(FVector(S + 0.02f));
    HighlightMesh->SetWorldLocation(Center);
    HighlightMesh->SetVisibility(true);

    const float Half = WorldScale::TileSizeCm * 0.5f + 0.5f;
    DrawDebugBox(GetWorld(), Center, FVector(Half), FQuat::Identity,
                 FColor::Cyan, false, 0.05f, 0, 1.5f);

    // 整次採掘的影響範圍外接框（黃色）：對全部 tile 算 world-space bounding box，
    // 概括這次按下去會挖掉的整個體積，不是只有中心那 1 格
    if (Tiles.Num() > 1)
    {
        FBox Bounds(ForceInit);
        for (const FGridPos& T : Tiles)
            Bounds += WorldScale::TileToWorld(T, TileWorld.Height);
        const FVector Margin(WorldScale::TileSizeCm * 0.5f);
        DrawDebugBox(GetWorld(), Bounds.GetCenter(), Bounds.GetExtent() + Margin,
                     FQuat::Identity, FColor::Yellow, false, 0.05f, 0, 2.f);
    }
}

void AVoxelWorldActor::HideHighlight()
{
    if (HighlightMesh)
        HighlightMesh->SetVisibility(false);
}

// Floor division correct for negative numerators.
int32 AVoxelWorldActor::MegaFloorDiv(int32 a, int32 b)
{
    return a / b - (a % b != 0 && (a ^ b) < 0 ? 1 : 0);
}

// ============================================================
// BeginPlay
// ============================================================

void AVoxelWorldActor::BeginPlay()
{
    Super::BeginPlay();
    InitializeWorldState();
}

void AVoxelWorldActor::ReinitializeForWorld(int32 NewWorldSeed, const FString& NewWorldSaveDir)
{
    // 清空舊世界留在記憶體裡的狀態，避免新世界混用舊世界的 chunk/放置物件登記
    // （見 .h ReinitializeForWorld 註解：這是「Saved/Worlds 永遠都同一個」回報的根因）。
    TileWorld.ClearAllChunks();
    PlacedRegistry = FPlacedObjectRegistry();
    CreatedMegaChunks.Empty();
    HideHighlight();

    WorldSeed    = NewWorldSeed;
    WorldSaveDir = NewWorldSaveDir;

    InitializeWorldState();
}

void AVoxelWorldActor::InitializeWorldState()
{
    TileWorld.Width     = WorldWidth;
    TileWorld.Height    = WorldHeight;
    TileWorld.Depth     = WorldDepth;
    TileWorld.WorldSeed = WorldSeed;

    // M-7: absolute save directory (empty WorldSaveDir = disable disk I/O).
    const FString AbsWorldDir = WorldSaveDir.IsEmpty()
        ? FString()
        : FPaths::ProjectSavedDir() / TEXT("Worlds") / WorldSaveDir;

    Streaming.Init(WorldWidth, WorldHeight, WorldDepth, WorldSeed, AbsWorldDir);

    // D-3：將 TileWorld 的爆炸聚合事件轉發給外部訂閱者（UDroppedItemManager 等）
    TileWorld.OnExplodeComplete = [this](FIntVector Center, const TMap<EMaterialType, int32>& Map)
    {
        OnExplosionComplete.Broadcast(Center, Map);
    };

    // M-10 Phase 3：世界建立後初始化一次 GPU CA 模擬器（RHI 不可用時 IsAvailable()=false，
    // Tick() 會整段跳過 GPU 路徑，照舊全 CPU 模擬，不會出錯）。
    //
    // 2026-06-22 暫時停用：使用者實機 PIE 回報「遊戲嚴重延遲、卡頓」。追查後發現
    // TileWorld3D::TickGpuZone() 每個 tick 都會對整個 GPU zone（ZoneW×ZoneH×ZoneD =
    // 128×256×128 = 4,194,304 格）做一次完整 GetCell() 掃描＋打包＋GPU
    // Upload→Simulate→Download（含同步 FlushRenderingCommands() GPU stall），這是
    // Phase 0~3 單元測試只用 4×4×4 小 zone 測過、從未在真實 ZoneW/H/D 尺度量過效能的
    // 熱路徑——原計畫就是把效能驗證留到 Phase 4（見 docs/plan-m10-gpu-ca.md），但這次
    // 在 BeginPlay() 呼叫 InitGpu() 讓它在 Phase 4 驗證完成前就提前在正式環境跑了起來。
    // 先不呼叫 InitGpu()，讓 GpuSim.IsAvailable() 維持 false，Tick() 整段跳過 GPU 路徑、
    // 照舊全 CPU 模擬（跟 Phase 3 之前一樣安全），等 Phase 4 把 TickGpuZone() 改成不要
    // 每 tick 全量掃描（例如只在 zone 真的移動時重建、或分批次跑）後再重新啟用。
    // TileWorld.InitGpu();

    // AR-B 保險措施：Blueprint 子類可能將 TileMaterialRegistry 覆蓋為 null（若 BP 建立時
    // 尚未有此欄位、或 BeginPlay 前尚未熱重載），在此強制重載。
    if (!TileMaterialRegistry)
        TileMaterialRegistry = LoadObject<UTileMaterialRegistry>(nullptr,
            TEXT("/Game/Data/DA_TileMaterialRegistry.DA_TileMaterialRegistry"));

    // M-6 / AR-B RMC init.
    // 每個 EMaterialType 值對應一個 material slot（PolyGroup = MaterialID）。
    // Registry 未建立或某 slot 為 null 時，fallback 到 VoxelMaterial。
    RMCMesh = RMCComp->InitializeRealtimeMesh<URealtimeMeshSimple>();
    const int32 MatCount = static_cast<int32>(EMaterialType::Count);
    for (int32 i = 0; i < MatCount; ++i)
    {
        UMaterialInterface* Mat = nullptr;
        if (TileMaterialRegistry)
            Mat = TileMaterialRegistry->GetSurface(static_cast<EMaterialType>(i));
        if (!Mat)
            Mat = VoxelMaterial;
        RMCMesh->SetupMaterialSlot(i, *FString::Printf(TEXT("TileMat_%d"), i), Mat);
    }
}

// ============================================================
// Tick
// ============================================================

void AVoxelWorldActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ── Player chunk coordinate (tile→chunk conversion) ────────
    FVector Loc = FVector::ZeroVector;
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        if (APawn* P = PC->GetPawn())
            Loc = P->GetActorLocation();

    // Actor location is in cm; one tile = TileSizeCm cm; one chunk = ChunkSize tiles.
    // UE5→voxel: X→X, Y→Z(depth), Z→-Y+WorldH(vertical inverted)
    const float ChunkSizeCm = WorldScale::TileSizeCm * static_cast<float>(WorldScale::ChunkSize);
    const int32 PCX = FMath::FloorToInt(Loc.X / ChunkSizeCm);
    const int32 PCZ = FMath::FloorToInt(Loc.Y / ChunkSizeCm);
    const float VoxelY = static_cast<float>(TileWorld.Height) - Loc.Z / WorldScale::TileSizeCm;
    const int32 PCY = FMath::FloorToInt(VoxelY / static_cast<float>(WorldScale::ChunkSize));

    // ── M-7: stream (disk-load or generate) + CA sim ──────────
    Streaming.Tick(TileWorld, PCX, PCY, PCZ, /*StreamRadius=*/2, /*CARadius=*/2);
    // 每 128 幀執行一次（每幀跑 EvictFarChunks O(N) 太浪費；128 幀≈2s@60fps）
    if ((GFrameCounter & 0x7F) == 0)
        Streaming.SaveAndEvict(TileWorld, PCX, PCY, PCZ, /*KeepRadius=*/4);

    // ── M-6: rebuild dirty mega-chunks ─────────────────────────
    if (!RMCMesh) return;

    TSet<FIntVector> DirtyMegaChunks;
    for (const auto& Pair : TileWorld.GetActiveChunks())
    {
        if (Pair.Value && Pair.Value->bMeshNeedsRebuild)
        {
            const FIntVector& CC = Pair.Key;
            DirtyMegaChunks.Add(FIntVector(
                MegaFloorDiv(CC.X, WorldScale::MegaChunkMult),
                MegaFloorDiv(CC.Y, WorldScale::MegaChunkMult),
                MegaFloorDiv(CC.Z, WorldScale::MegaChunkMult)));
        }
    }

    for (const FIntVector& MC : DirtyMegaChunks)
        RebuildMegaChunk(MC);
}

// ============================================================
// TriggerVoxelDestruction（V-4）
// ============================================================

void AVoxelWorldActor::TriggerVoxelDestruction(FIntVector BoundsMin, FIntVector BoundsMax,
                                                EDestroyReason Reason, float Intensity, FVector SlashDir)
{
    TMap<EMaterialType, int32> DestroyedByMat;
    for (int X = BoundsMin.X; X <= BoundsMax.X; ++X)
    for (int Y = BoundsMin.Y; Y <= BoundsMax.Y; ++Y)
    for (int Z = BoundsMin.Z; Z <= BoundsMax.Z; ++Z)
    {
        EMaterialType Mat = TileWorld.GetTile(X, Y, Z);
        if (Mat == EMaterialType::Air) continue;
        TileWorld.SetTile(X, Y, Z, EMaterialType::Air);
        DestroyedByMat.FindOrAdd(Mat)++;
    }

    const FIntVector Center = (BoundsMin + BoundsMax) / 2;
    OnVoxelDestructionComplete.Broadcast(Center, DestroyedByMat, Reason, Intensity, SlashDir);
}

// ============================================================
// RebuildMegaChunk
// ============================================================

void AVoxelWorldActor::RebuildMegaChunk(FIntVector MC)
{
    using namespace RealtimeMesh;

    FRealtimeMeshStreamSet StreamSet = FGreedyMesher::Build(TileWorld, MC);

    const FName MCName(*FString::Printf(TEXT("MC_%d_%d_%d"), MC.X, MC.Y, MC.Z));
    const FRealtimeMeshSectionGroupKey GroupKey =
        FRealtimeMeshSectionGroupKey::Create(0, MCName);

    if (CreatedMegaChunks.Contains(MC))
        RMCMesh->UpdateSectionGroup(GroupKey, MoveTemp(StreamSet));
    else
    {
        RMCMesh->CreateSectionGroup(GroupKey, MoveTemp(StreamSet));
        CreatedMegaChunks.Add(MC);
    }

    const int32 MinCX = MC.X * WorldScale::MegaChunkMult;
    const int32 MinCY = MC.Y * WorldScale::MegaChunkMult;
    const int32 MinCZ = MC.Z * WorldScale::MegaChunkMult;

    for (int32 cx = MinCX; cx < MinCX + WorldScale::MegaChunkMult; ++cx)
    for (int32 cy = MinCY; cy < MinCY + WorldScale::MegaChunkMult; ++cy)
    for (int32 cz = MinCZ; cz < MinCZ + WorldScale::MegaChunkMult; ++cz)
    {
        FChunk3D* Chunk = TileWorld.GetChunkAt(FIntVector(cx, cy, cz));
        if (Chunk) Chunk->bMeshNeedsRebuild = false;
    }
}
