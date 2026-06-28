#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "TileWorld3D.h"
#include "ChunkStreamingManager.h"
#include "RealtimeMeshComponent.h"
#include "RealtimeMeshSimple.h"
#include "GridPos.h"
#include "TileMaterialRegistry.h"
#include "PlacedObjectRegistry.h"
#include "MaterialType.h"
#include "AVoxelWorldActor.generated.h"

// TMap 含逗號無法直接放入 DECLARE_MULTICAST_DELEGATE 宏，先 typedef
typedef TMap<EMaterialType, int32> FMaterialCountMap;

// D-3：爆炸聚合事件，SkillCreatorRuntime 訂閱後呼叫 SpawnDebris
// 避免 VoxelWorld 直接依賴 SkillCreatorRuntime（循環依賴）
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnExplosionCompleteDelegate,
    FIntVector, const FMaterialCountMap&);

// V-4：Route B 體素注入後的銷毀事件（含完整毀壞參數，供 SpawnDebris 使用）
DECLARE_MULTICAST_DELEGATE_FiveParams(FOnVoxelDestructionCompleteDelegate,
    FIntVector, const FMaterialCountMap&, EDestroyReason, float, FVector);

// DECO-2：chunk 生成/淘汰通知（UDecorationSubsystem 訂閱後管理 ISMC 裝飾）
DECLARE_MULTICAST_DELEGATE_OneParam(FOnChunkAppliedDelegate, FIntVector);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnChunkEvictedDelegate, FIntVector);

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

    // P-18: 可穿越格（PlatformType=1）獨立渲染組件，碰撞完全關閉
    UPROPERTY(VisibleAnywhere, Category="VoxelWorld|Rendering")
    TObjectPtr<URealtimeMeshComponent> RMCPassthroughComp;

    // ── Accessors ─────────────────────────────────────────────────
    FTileWorld3D* GetTileWorld() { return &TileWorld; }

    // CreateWorld 首次進入時的出生點地形預生成用（GameMode::StartGameplayWithWorld）
    FMapGenerator3D& GetMapGenerator() { return Streaming.GetMapGenerator(); }

    // K-5：玩家放置物件登記（完美移除分流用，ASkillCreatorCharacter::OnMine/OnPlace）
    FPlacedObjectRegistry& GetPlacedRegistry() { return PlacedRegistry; }

    static AVoxelWorldActor* FindInWorld(UWorld* World);

    // D-3：爆炸聚合事件（SkillCreatorRuntime 的 UDroppedItemManager 在 OnWorldBeginPlay 訂閱）
    FOnExplosionCompleteDelegate OnExplosionComplete;

    // V-4：Route B 體素注入後銷毀事件（UDroppedItemManager 在 OnWorldBeginPlay 訂閱）
    FOnVoxelDestructionCompleteDelegate OnVoxelDestructionComplete;

    // DECO-2：chunk 事件（UDecorationSubsystem 在 OnWorldBeginPlay 訂閱）
    FOnChunkAppliedDelegate OnChunkApplied;
    FOnChunkEvictedDelegate OnChunkEvicted;

    // V-4：體素注入後的 tile 銷毀（由 ADestructibleMeshActor::TriggerDestruction 呼叫）
    void TriggerVoxelDestruction(FIntVector BoundsMin, FIntVector BoundsMax,
                                 EDestroyReason Reason, float Intensity, FVector SlashDir);

    // 2026-06-23 修復：AVoxelWorldActor 是動態 SpawnActor 出來的單一實例（不是隨關卡重載），
    // 在同一個遊戲進程內換到第二個世界時（從選單回去再建一個新世界），原本的程式碼完全
    // 沒有任何地方更新這個既存 Actor 的 WorldSeed/WorldSaveDir，BeginPlay() 的初始化也只在
    // 第一次 Spawn 時跑過一次——導致玩家後續創建的任何「新世界」其實全部還是用第一個世界的
    // seed + 存檔目錄在跑（ASkillCreatorGameMode::SpawnWorldAndMobs 的 `if (!VW)` 判斷讓
    // 既存 Actor 完全沒被重新設定），這正是「Saved/Worlds 永遠都同一個」回報的根因。
    // 換世界時呼叫這個函式：清空舊世界的記憶體狀態（chunk/已渲染網格/已放置物件/串流快取），
    // 重新套用新的 Seed/SaveDir 並重跑一次等同 BeginPlay() 的初始化。
    void ReinitializeForWorld(int32 NewWorldSeed, const FString& NewWorldSaveDir);

    // ── 採掘高亮 ──────────────────────────────────────────────────
    // 2026-06-23 修正：原本只能顯示「準心對準的單一格」，但實際一次採掘會摧毀整個形狀
    // 範圍（預設 Cube 半徑 5 = 11×11×11 = 1331 格，對應 Godot Main.cs:71-72「1 material
    // unit」設計），單格高亮跟真正會被挖掉的範圍完全不成比例。改成傳入整個 tile 陣列：
    // Tiles[0] 視為準心瞄準的中心格（沿用原本的單格 Cube mesh + 青色線框），其餘元素用來
    // 算一個外接框（黃色線框）概括整次採掘的影響範圍。不逐格畫線框（形狀範圍大時會變成
    // 密密麻麻看不出輪廓，Godot 用 SurfaceTool 逐格面剔除才能畫出乾淨外框，這裡用 bounding
    // box 近似——Cube 形狀時這就是精確邊界，Sphere/Cylinder 等圓形形狀則是寬鬆外接框，
    // 仍能讓玩家掌握大致影響範圍，避免引入額外的程序網格生成複雜度）。
    // 傳空陣列＝隱藏。
    void ShowHighlight(const TArray<FGridPos>& Tiles);
    void HideHighlight();

    // ── AActor ────────────────────────────────────────────────────
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // W-E：草地回復查詢（供 OnTileDestroyed 或挖掘後呼叫）
    void NotifyDirtExposed(FIntVector TilePos);

private:
    FTileWorld3D           TileWorld;
    FChunkStreamingManager Streaming;
    FPlacedObjectRegistry  PlacedRegistry;

    UPROPERTY() TObjectPtr<UStaticMeshComponent> HighlightMesh;

    TObjectPtr<URealtimeMeshSimple> RMCMesh;
    TSet<FIntVector> CreatedMegaChunks;

    // P-18: passthrough mesh state
    TObjectPtr<URealtimeMeshSimple> RMCPassthroughMesh;
    TSet<FIntVector> CreatedPassthroughChunks;

    // DECO-2：上幀 active chunk 快照，用 set diff 偵測新增/淘汰 chunk 並廣播 delegate
    TSet<FIntVector> PrevActiveChunks;

    void RebuildMegaChunk(FIntVector MegaChunkCoord);
    static int32 MegaFloorDiv(int32 a, int32 b);

    // BeginPlay() 跟 ReinitializeForWorld() 共用的初始化邏輯（TileWorld 尺寸/Seed、
    // ChunkStreamingManager::Init、material slot 設定），抽出來避免兩處各自維護一份。
    void InitializeWorldState();

    // W-E：草地回復系統（Dirt_Dry + Air above → 180 秒後回復為 Grass）
    struct FRegrowthEntry { FIntVector Pos; float TimeLeft; };
    TArray<FRegrowthEntry> GrassRegrowthQueue;
    void TickGrassRegrowth(float DeltaTime);

    // P-5：發光格點光源管理（per-MegaChunk，每 16³ tile 區域最多一個光源）
    TMap<FIntVector, TArray<UPointLightComponent*>> MCLightMap;
    void RebuildChunkLights(FIntVector MegaChunkCoord);
    void ClearChunkLights(FIntVector MegaChunkCoord);
    void ClearAllChunkLights();

    // Bug-4 修復：防止同一 MegaChunk 連續重建（記錄上次重建時間，最短間距 0.5s）
    TMap<FIntVector, double> MCLastRebuildTime;
};
