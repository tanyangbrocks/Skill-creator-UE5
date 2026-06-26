#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AWorldUnit.generated.h"

class AVoxelWorldActor;

// 世界固定物件基底類（可放置物 + 可採集物的共同祖先）
// 提供：AnchorTile tile 座標、MeshComp 渲染元件、VoxelWorld 快取、tile 變化虛擬鉤子。
// 子類：APlacedFixtureActor（可放置物）/ AWeedEntity（可採集物）
UCLASS(Abstract)
class SKILLCREATORRUNTIME_API AWorldUnit : public AActor
{
    GENERATED_BODY()
public:
    AWorldUnit();

    // 此物件錨定的 tile 座標（由生成方在 SpawnActor 後設定）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WorldUnit")
    FIntVector AnchorTile = FIntVector::ZeroValue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WorldUnit")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    // 子類覆寫：AnchorTile 所在 tile 發生變化時的反應
    // 目前由子類 Tick 輪詢觸發；未來可改由 AVoxelWorldActor 主動廣播
    virtual void OnAnchorTileChanged() {}

protected:
    virtual void BeginPlay() override;

    // 供子類查詢 tile 世界（GetAllActorsOfClass 只在 BeginPlay 時查一次）
    TWeakObjectPtr<AVoxelWorldActor> CachedVoxelWorld;
};
