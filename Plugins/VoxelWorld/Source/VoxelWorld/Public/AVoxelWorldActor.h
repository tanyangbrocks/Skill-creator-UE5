#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "TileWorld3D.h"
#include "ChunkStreamingManager.h"
#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"
#include "GridPos.h"
#include "TileMaterialRegistry.h"
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
    // Fallback：無 Registry 或 Registry 缺某材質時使用
    UPROPERTY(EditAnywhere, Category="VoxelWorld|Rendering")
    TObjectPtr<UMaterialInterface> VoxelMaterial;

    // AR-B：每種 EMaterialType 對應獨立材質
    // 自動從 /Game/Data/DA_TileMaterialRegistry 載入；為 null 時全 fallback 到 VoxelMaterial
    UPROPERTY(EditAnywhere, Category="VoxelWorld|Rendering")
    TObjectPtr<UTileMaterialRegistry> TileMaterialRegistry;

    UPROPERTY(VisibleAnywhere, Category="VoxelWorld|Rendering")
    TObjectPtr<URealtimeMeshComponent> RMCComp;

    // ── Accessors ─────────────────────────────────────────────────
    FTileWorld3D* GetTileWorld() { return &TileWorld; }

    // CreateWorld 首次進入時的出生點地形預生成用（GameMode::StartGameplayWithWorld）
    FMapGenerator3D& GetMapGenerator() { return Streaming.GetMapGenerator(); }

    static AVoxelWorldActor* FindInWorld(UWorld* World);

    // ── 採掘高亮 ──────────────────────────────────────────────────
    // 在 tile 格心顯示半透明線框立方體；不傳座標則隱藏
    void ShowHighlight(FGridPos TilePos);
    void HideHighlight();

    // ── AActor ────────────────────────────────────────────────────
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    FTileWorld3D           TileWorld;
    FChunkStreamingManager Streaming;

    UPROPERTY() TObjectPtr<UStaticMeshComponent> HighlightMesh;

    TObjectPtr<URealtimeMeshSimple> RMCMesh;
    TSet<FIntVector> CreatedMegaChunks;

    void RebuildMegaChunk(FIntVector MegaChunkCoord);
    static int32 MegaFloorDiv(int32 a, int32 b);
};
