#include "AVoxelizableActor.h"

void AVoxelizableActor::NotifyMovementChanged(bool bMovingNow)
{
    if (bIsMoving == bMovingNow) return;
    bIsMoving = bMovingNow;
    ReevaluatePath();
}

void AVoxelizableActor::NotifyScaleChanged()
{
    ReevaluatePath();
}

float AVoxelizableActor::ComputeFidelityTiles() const
{
    const FBox Box = GetComponentsBoundingBox(/*bNonColliding=*/false);
    const float MinDim = Box.GetSize().GetMin();
    if (MinDim <= 0.f) return 0.f;
    return MinDim / WorldScale::TileSizeCm;
}

void AVoxelizableActor::ReevaluatePath()
{
    switch (VoxelizationMode)
    {
    case EVoxelizationMode::ForceAlwaysVoxel:
        if (!bIsFullVoxel) { bIsFullVoxel = true; SwitchToFullVoxel(); }
        break;

    case EVoxelizationMode::Auto:
    {
        const bool bShouldBeVoxel = !bIsMoving &&
            ComputeFidelityTiles() >= static_cast<float>(WorldScale::MinFidelityTiles);
        if (bShouldBeVoxel && !bIsFullVoxel)  { bIsFullVoxel = true;  SwitchToFullVoxel(); }
        if (!bShouldBeVoxel && bIsFullVoxel)  { bIsFullVoxel = false; SwitchToMeshDisplay(); }
        break;
    }

    case EVoxelizationMode::ForceMeshWithVoxel:
        if (bIsFullVoxel) { bIsFullVoxel = false; SwitchToMeshDisplay(); }
        break;

    case EVoxelizationMode::PureMesh:
        break; // 不做任何切換
    }
}
