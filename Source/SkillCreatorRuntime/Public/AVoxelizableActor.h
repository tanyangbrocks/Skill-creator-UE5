#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldScale.h"
#include "AVoxelizableActor.generated.h"

// 體素化路徑模式（plan-voxelization.md §十六）
UENUM(BlueprintType)
enum class EVoxelizationMode : uint8
{
    ForceAlwaysVoxel    UMETA(DisplayName="ForceAlwaysVoxel"),   // 路線 A 強制：地形/建築
    Auto                UMETA(DisplayName="Auto"),               // 系統依精細度+移動狀態決定
    ForceMeshWithVoxel  UMETA(DisplayName="ForceMeshWithVoxel"), // 路線 B 強制：小型道具
    PureMesh            UMETA(DisplayName="PureMesh"),           // 永遠 Mesh，不體素化
};

// 所有「需要 Runtime 評估渲染路徑」的 Actor 基底類別（plan-voxelization.md §九）
// 子類須實作 SwitchToFullVoxel() 和 SwitchToMeshDisplay()
UCLASS(Abstract)
class SKILLCREATORRUNTIME_API AVoxelizableActor : public AActor
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voxelization")
    EVoxelizationMode VoxelizationMode = EVoxelizationMode::Auto;

    // 外部通知：移動狀態改變（角色 locomotion、定身效果等）
    void NotifyMovementChanged(bool bMovingNow);

    // 外部通知：ActorScale 改變（放大/縮小效果觸發重評估）
    void NotifyScaleChanged();

protected:
    // 子類實作：切換到全體素展示（Route A）
    virtual void SwitchToFullVoxel()   PURE_VIRTUAL(AVoxelizableActor::SwitchToFullVoxel,);
    // 子類實作：切換回 Mesh 展示（Route B）
    virtual void SwitchToMeshDisplay() PURE_VIRTUAL(AVoxelizableActor::SwitchToMeshDisplay,);

    bool bIsFullVoxel = false;
    bool bIsMoving    = false;

private:
    void  ReevaluatePath();
    float ComputeFidelityTiles() const;
};
