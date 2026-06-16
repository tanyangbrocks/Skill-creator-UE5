#include "AVoxelWorldActor.h"
#include "GreedyMesher.h"
#include "WorldScale.h"
#include "Chunk3D.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"

// ============================================================
// 構造
// ============================================================

AVoxelWorldActor::AVoxelWorldActor()
{
    PrimaryActorTick.bCanEverTick = true;

    RMCComp = CreateDefaultSubobject<URealtimeMeshComponent>(TEXT("RMC"));
    SetRootComponent(RMCComp);

    // 自動載入 M_Voxel 材質，不需要在 Editor 手動指定
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
        TEXT("/Game/M_Voxel.M_Voxel"));
    if (MatFinder.Succeeded())
        VoxelMaterial = MatFinder.Object;
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

    TileWorld.Width     = WorldWidth;
    TileWorld.Height    = WorldHeight;
    TileWorld.Depth     = WorldDepth;
    TileWorld.WorldSeed = WorldSeed;

    // M-7: absolute save directory (empty WorldSaveDir = disable disk I/O).
    const FString AbsWorldDir = WorldSaveDir.IsEmpty()
        ? FString()
        : FPaths::ProjectSavedDir() / TEXT("Worlds") / WorldSaveDir;

    Streaming.Init(WorldWidth, WorldHeight, WorldDepth, WorldSeed, AbsWorldDir);

    // M-6 RMC init.
    RMCMesh = RMCComp->InitializeRealtimeMesh<URealtimeMeshSimple>();
    RMCMesh->SetupMaterialSlot(0, TEXT("VoxelMaterial"), VoxelMaterial);
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
