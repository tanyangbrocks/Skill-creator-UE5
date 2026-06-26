#pragma once
#include "CoreMinimal.h"
#include "RealtimeMeshSimple.h"

class FTileWorld3D;

// P-18: controls which PlatformType tiles are included in the stream set.
// SolidOnly (default): PlatformType=0 tiles → visual + collision mesh
// PassthroughOnly:     PlatformType=1 tiles → visual-only mesh (RMCPassthroughComp 無碰撞)
enum class EGreedyMeshFilter : uint8 { SolidOnly = 0, PassthroughOnly = 1 };

// Pure-C++ greedy mesh builder.
// Scans the 64³ tile region (= one Mega-Chunk) and merges same-material faces
// into the fewest quads. All tile reads go through FTileWorld3D::GetTile()
// which returns Air for unloaded chunks — safe for both interior and boundary faces.
class VOXELWORLD_API FGreedyMesher
{
public:
    // MegaChunkCoord in mega-chunk space.
    // World tile range = [Coord * 64, (Coord+1) * 64) per axis.
    // Returns a FRealtimeMeshStreamSet with Position / Tangents / TexCoords /
    // Color (R=MaterialID) / Triangles / PolyGroups(每面 = MaterialID) streams.
    // Empty StreamSet if the whole region has no tiles matching the filter.
    static RealtimeMesh::FRealtimeMeshStreamSet Build(
        const FTileWorld3D& World,
        FIntVector          MegaChunkCoord,
        EGreedyMeshFilter   Filter = EGreedyMeshFilter::SolidOnly);
};
