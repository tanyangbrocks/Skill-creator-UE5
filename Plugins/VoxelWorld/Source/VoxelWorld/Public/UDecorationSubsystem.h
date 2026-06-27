#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DecorationRegistry.h"
#include "UDecorationSubsystem.generated.h"

class AVoxelWorldActor;
class FTileWorld3D;

// DECO-3：地表裝飾管理器（UWorldSubsystem）
// 訂閱 AVoxelWorldActor::OnChunkApplied/OnChunkEvicted，
// 用 ISMC（Instanced Static Mesh）批次管理地表裝飾物件。
// 新增裝飾物件：只需在 FDecorationRegistry::Initialize() 加 Register()，此系統自動接管。
UCLASS()
class VOXELWORLD_API UDecorationSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

private:
    // PropId → ISMC（一種 Mesh 一個 ISMC，跨所有 chunk 共用）
    UPROPERTY() TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> ISMCByProp;

    // 每個 chunk 放置了哪些裝飾（PropId + 世界變換），供 eviction 時重建用
    TMap<FIntVector, TArray<TPair<FName, FTransform>>> ChunkTransforms;

    UPROPERTY() TWeakObjectPtr<AVoxelWorldActor> CachedVWA;

    FDelegateHandle ChunkAppliedHandle;
    FDelegateHandle ChunkEvictedHandle;

    // ── chunk 事件 ──────────────────────────────────────────────────────
    void OnChunkApplied(FIntVector CC);
    void OnChunkEvicted(FIntVector CC);

    // ── 內部輔助 ────────────────────────────────────────────────────────
    UInstancedStaticMeshComponent* GetOrCreateISMC(const FDecorationDef& Def);
    void RebuildAllISMCs();

    // 確定性 hash 決定是否放置裝飾（DECO-4）
    bool ShouldPlace(FIntVector CC, int32 LX, int32 LZ,
                     const FDecorationDef& Def, float& OutScale) const;
};
