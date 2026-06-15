#pragma once
#include "CoreMinimal.h"
#include "RealtimeMeshSimple.h"

class FTileWorld3D;

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
    // Color (R=MaterialID) / Triangles / PolyGroups(all 0) streams.
    // Empty StreamSet if the whole region is air.
    static RealtimeMesh::FRealtimeMeshStreamSet Build(
        const FTileWorld3D& World,
        FIntVector          MegaChunkCoord);
};
