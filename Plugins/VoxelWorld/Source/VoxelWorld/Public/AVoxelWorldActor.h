#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TileWorld3D.h"
#include "ChunkStreamingManager.h"
#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"
#include "AVoxelWorldActor.generated.h"

// UE Actor wrapper for FTileWorld3D + RMC rendering.
// M-6: Greedy-mesh RMC rendering per Mega-Chunk.
// M-7: ChunkStreamingManager handles disk-load-first + save-on-eviction.
//      Set WorldSaveDir in Editor; save/load is triggered via FFlowSaveSystem.
UCLASS()
class VOXELWORLD_API AVoxelWorldActor : public AActor
{
    GENERATED_BODY()
public:
    AVoxelWorldActor();

    // ── World settings ─────────────────────────────────────────────
    UPROPERTY(EditAnywhere, Category="VoxelWorld")
    int32 WorldSeed = 12345;

    UPROPERTY(EditAnywhere, Category="VoxelWorld")
    int32 WorldWidth  = 0;   // 0 = infinite lazy chunks

    UPROPERTY(EditAnywhere, Category="VoxelWorld")
    int32 WorldHeight = 256;

    UPROPERTY(EditAnywhere, Category="VoxelWorld")
    int32 WorldDepth  = 0;

    // Relative path under {ProjectSaved}/Worlds/ for chunk + meta files.
    // Leave empty to disable disk save/load (generate-only mode).
    UPROPERTY(EditAnywhere, Category="VoxelWorld")
    FString WorldSaveDir = TEXT("World_0001");

    // ── Rendering (M-6) ───────────────────────────────────────────
    UPROPERTY(EditAnywhere, Category="VoxelWorld|Rendering")
    TObjectPtr<UMaterialInterface> VoxelMaterial;

    UPROPERTY(VisibleAnywhere, Category="VoxelWorld|Rendering")
    TObjectPtr<URealtimeMeshComponent> RMCComp;

    // ── Accessors ─────────────────────────────────────────────────
    FTileWorld3D* GetTileWorld() { return &TileWorld; }

    static AVoxelWorldActor* FindInWorld(UWorld* World);

    // ── AActor ────────────────────────────────────────────────────
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    FTileWorld3D           TileWorld;
    FChunkStreamingManager Streaming;

    TObjectPtr<URealtimeMeshSimple> RMCMesh;
    TSet<FIntVector> CreatedMegaChunks;

    void RebuildMegaChunk(FIntVector MegaChunkCoord);
    static int32 MegaFloorDiv(int32 a, int32 b);
};
