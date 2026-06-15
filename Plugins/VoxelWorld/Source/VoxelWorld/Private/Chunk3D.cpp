#include "Chunk3D.h"

FChunk3D::FChunk3D(FIntVector InCoord)
    : ChunkCoord(InCoord)
{
    FMemory::Memzero(Cells,   sizeof(Cells));
    FMemory::Memzero(Updated, sizeof(Updated));
}

void FChunk3D::MarkDirty(int32 lx, int32 ly, int32 lz)
{
    bDirty            = true;
    bMeshNeedsRebuild = true;
    bNeedsSave        = true;
    DirtyMin[0] = FMath::Min(DirtyMin[0], lx);
    DirtyMin[1] = FMath::Min(DirtyMin[1], ly);
    DirtyMin[2] = FMath::Min(DirtyMin[2], lz);
    DirtyMax[0] = FMath::Max(DirtyMax[0], lx);
    DirtyMax[1] = FMath::Max(DirtyMax[1], ly);
    DirtyMax[2] = FMath::Max(DirtyMax[2], lz);
}

void FChunk3D::FlagMeshRebuild()
{
    bMeshNeedsRebuild = true;
}

void FChunk3D::ClearDirty()
{
    bDirty      = false;
    DirtyMin[0] = DirtyMin[1] = DirtyMin[2] = Size;
    DirtyMax[0] = DirtyMax[1] = DirtyMax[2] = -1;
}

void FChunk3D::ClearUpdated()
{
    FMemory::Memzero(Updated, sizeof(Updated));
}
