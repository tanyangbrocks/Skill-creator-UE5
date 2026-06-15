#include "GreedyMesher.h"
#include "TileWorld3D.h"
#include "MaterialType.h"
#include "WorldScale.h"
#include "RealtimeMeshSimple.h"

using namespace RealtimeMesh;

// Air, Fire, Steam do not occlude neighbouring faces.
static bool IsOpaque(uint8 MatID)
{
    const EMaterialType M = static_cast<EMaterialType>(MatID);
    return M != EMaterialType::Air
        && M != EMaterialType::Fire
        && M != EMaterialType::Steam;
}

FRealtimeMeshStreamSet FGreedyMesher::Build(const FTileWorld3D& World,
                                             FIntVector          MegaChunkCoord)
{
    constexpr int32 MegaSize = WorldScale::MegaChunkMult * WorldScale::ChunkSize; // 64
    const float     S        = WorldScale::TileSizeCm;                             // cm per tile
    const FIntVector MinTile  = MegaChunkCoord * MegaSize;
    const FVector3f  Origin(MinTile.X * S, MinTile.Y * S, MinTile.Z * S);

    // ── Stream setup ─────────────────────────────────────────────────────────
    FRealtimeMeshStreamSet StreamSet;

    TRealtimeMeshStreamBuilder<FVector3f>
        PosBuilder(StreamSet.AddStream(FRealtimeMeshStreams::Position,
                                       GetRealtimeMeshBufferLayout<FVector3f>()));

    TRealtimeMeshStreamBuilder<FRealtimeMeshTangentsHighPrecision,
                               FRealtimeMeshTangentsNormalPrecision>
        TangentBuilder(StreamSet.AddStream(FRealtimeMeshStreams::Tangents,
                                           GetRealtimeMeshBufferLayout<FRealtimeMeshTangentsNormalPrecision>()));

    TRealtimeMeshStreamBuilder<FVector2f, FVector2DHalf>
        UVBuilder(StreamSet.AddStream(FRealtimeMeshStreams::TexCoords,
                                      GetRealtimeMeshBufferLayout<FVector2DHalf>()));

    TRealtimeMeshStreamBuilder<FColor>
        ColorBuilder(StreamSet.AddStream(FRealtimeMeshStreams::Color,
                                         GetRealtimeMeshBufferLayout<FColor>()));

    TRealtimeMeshStreamBuilder<TIndex3<uint32>, TIndex3<uint16>>
        TriBuilder(StreamSet.AddStream(FRealtimeMeshStreams::Triangles,
                                       GetRealtimeMeshBufferLayout<TIndex3<uint16>>()));

    TRealtimeMeshStreamBuilder<uint32, uint16>
        PolyGroupBuilder(StreamSet.AddStream(FRealtimeMeshStreams::PolyGroups,
                                             GetRealtimeMeshBufferLayout<uint16>()));

    // ── Mask buffer (reused for every slice) ─────────────────────────────────
    TArray<uint8> Mask;
    Mask.SetNumUninitialized(MegaSize * MegaSize);

    // ── 6 face directions ────────────────────────────────────────────────────
    // d  = slice axis  (0=X, 1=Y, 2=Z)
    // u  = first perpendicular axis  (d+1)%3
    // v  = second perpendicular axis (d+2)%3
    // facing: +1 → positive-d face, -1 → negative-d face
    for (int32 d = 0; d < 3; ++d)
    {
        const int32 u = (d + 1) % 3;
        const int32 v = (d + 2) % 3;

        for (int32 facing = -1; facing <= 1; facing += 2)
        {
            for (int32 s = 0; s < MegaSize; ++s)
            {
                // ── Build visibility mask for this slice ──────────────────
                for (int32 i = 0; i < MegaSize; ++i)
                {
                    for (int32 j = 0; j < MegaSize; ++j)
                    {
                        // Self tile in mega-chunk local coords
                        FIntVector Self(0, 0, 0);
                        Self[d] = s; Self[u] = i; Self[v] = j;

                        // Neighbour tile (one step in the facing direction)
                        FIntVector Nbr = Self;
                        Nbr[d] += facing;

                        const uint8 SelfMat = static_cast<uint8>(
                            World.GetTile(MinTile.X + Self.X,
                                          MinTile.Y + Self.Y,
                                          MinTile.Z + Self.Z));

                        // Neighbour may cross the mega-chunk boundary;
                        // GetTile() returns Air for unloaded chunks — correct.
                        const uint8 NbrMat = static_cast<uint8>(
                            World.GetTile(MinTile.X + Nbr.X,
                                          MinTile.Y + Nbr.Y,
                                          MinTile.Z + Nbr.Z));

                        Mask[i * MegaSize + j] =
                            (IsOpaque(SelfMat) && !IsOpaque(NbrMat)) ? SelfMat : 0;
                    }
                }

                // ── Greedy merge ──────────────────────────────────────────
                for (int32 i = 0; i < MegaSize; ++i)
                {
                    int32 j = 0;
                    while (j < MegaSize)
                    {
                        const uint8 Mat = Mask[i * MegaSize + j];
                        if (Mat == 0) { ++j; continue; }

                        // Extend width (v direction, j)
                        int32 w = 1;
                        while (j + w < MegaSize && Mask[i * MegaSize + j + w] == Mat)
                            ++w;

                        // Extend height (u direction, i)
                        int32 h = 1;
                        while (i + h < MegaSize)
                        {
                            bool bOk = true;
                            for (int32 k = 0; k < w; ++k)
                                if (Mask[(i + h) * MegaSize + j + k] != Mat)
                                    { bOk = false; break; }
                            if (!bOk) break;
                            ++h;
                        }

                        // ── Emit quad ─────────────────────────────────────
                        // Face plane position along d (in cm from mega-chunk origin)
                        const float face_d = static_cast<float>(facing == 1 ? s + 1 : s) * S;

                        FVector3f Normal(0.f, 0.f, 0.f); Normal[d] = static_cast<float>(facing);
                        FVector3f Tangent(0.f, 0.f, 0.f); Tangent[u] = 1.f;
                        const FRealtimeMeshTangentsHighPrecision Tang(Normal, Tangent);
                        const FColor Col(Mat, 0, 0, 255); // R = MaterialID

                        // 4 corners: u ∈ [i, i+h], v ∈ [j, j+w]
                        auto MakePos = [&](float uc, float vc) -> FVector3f
                        {
                            FVector3f P(0.f, 0.f, 0.f);
                            P[d] = face_d;
                            P[u] = uc * S;
                            P[v] = vc * S;
                            return Origin + P;
                        };

                        const int32 V0 = PosBuilder.Add(MakePos(    i,     j)); TangentBuilder.Add(Tang); UVBuilder.Add(FVector2f(       0.f,        0.f)); ColorBuilder.Add(Col);
                        const int32 V1 = PosBuilder.Add(MakePos(i + h,     j)); TangentBuilder.Add(Tang); UVBuilder.Add(FVector2f((float)h,        0.f)); ColorBuilder.Add(Col);
                        const int32 V2 = PosBuilder.Add(MakePos(i + h, j + w)); TangentBuilder.Add(Tang); UVBuilder.Add(FVector2f((float)h, (float)w)); ColorBuilder.Add(Col);
                        const int32 V3 = PosBuilder.Add(MakePos(    i, j + w)); TangentBuilder.Add(Tang); UVBuilder.Add(FVector2f(       0.f, (float)w)); ColorBuilder.Add(Col);

                        // Winding: CCW gives outward normal via right-hand rule.
                        // facing=+1: cross(V1-V0, V2-V0) ∝ +d ✓
                        // facing=-1: reversed winding → cross ∝ -d ✓
                        if (facing == 1)
                        {
                            TriBuilder.Add(TIndex3<uint32>(V0, V1, V2));
                            TriBuilder.Add(TIndex3<uint32>(V0, V2, V3));
                        }
                        else
                        {
                            TriBuilder.Add(TIndex3<uint32>(V0, V2, V1));
                            TriBuilder.Add(TIndex3<uint32>(V0, V3, V2));
                        }
                        PolyGroupBuilder.Add(0);
                        PolyGroupBuilder.Add(0);

                        // Clear consumed mask cells
                        for (int32 di = 0; di < h; ++di)
                            for (int32 dj = 0; dj < w; ++dj)
                                Mask[(i + di) * MegaSize + j + dj] = 0;

                        j += w;
                    }
                }
            }
        }
    }

    return MoveTemp(StreamSet);
}
