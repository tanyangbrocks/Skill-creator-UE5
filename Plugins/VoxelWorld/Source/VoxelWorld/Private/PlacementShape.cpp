#include "PlacementShape.h"

TArray<FIntVector> FPlacementShape::GetOffsets(EPlacementShape Shape, int32 Radius)
{
    switch (Shape)
    {
    case EPlacementShape::Single:   return { FIntVector::ZeroValue };
    case EPlacementShape::Cube:     return CubeOffsets(FMath::Max(0, Radius));
    case EPlacementShape::Sphere:   return SphereOffsets(FMath::Max(0, Radius));
    case EPlacementShape::Cylinder: return CylinderOffsets(FMath::Max(0, Radius), FMath::Max(1, Radius * 2));
    case EPlacementShape::Flat:     return FlatOffsets(FMath::Max(0, Radius));
    default:                        return { FIntVector::ZeroValue };
    }
}

TArray<FIntVector> FPlacementShape::CubeOffsets(int32 Radius)
{
    TArray<FIntVector> Out;
    for (int32 x = -Radius; x <= Radius; ++x)
    for (int32 y = -Radius; y <= Radius; ++y)
    for (int32 z = -Radius; z <= Radius; ++z)
        Out.Add(FIntVector(x, y, z));
    return Out;
}

TArray<FIntVector> FPlacementShape::SphereOffsets(int32 Radius)
{
    TArray<FIntVector> Out;
    const int32 R2 = Radius * Radius;
    for (int32 x = -Radius; x <= Radius; ++x)
    for (int32 y = -Radius; y <= Radius; ++y)
    for (int32 z = -Radius; z <= Radius; ++z)
        if (x*x + y*y + z*z <= R2)
            Out.Add(FIntVector(x, y, z));
    return Out;
}

TArray<FIntVector> FPlacementShape::CylinderOffsets(int32 Radius, int32 Height)
{
    TArray<FIntVector> Out;
    const int32 R2 = Radius * Radius;
    const int32 HalfH = Height / 2;
    for (int32 x = -Radius; x <= Radius; ++x)
    for (int32 z = -Radius; z <= Radius; ++z)
    {
        if (x*x + z*z > R2) continue;
        for (int32 y = -HalfH; y <= HalfH; ++y)
            Out.Add(FIntVector(x, y, z));
    }
    return Out;
}

TArray<FIntVector> FPlacementShape::FlatOffsets(int32 Radius)
{
    TArray<FIntVector> Out;
    for (int32 x = -Radius; x <= Radius; ++x)
    for (int32 z = -Radius; z <= Radius; ++z)
        Out.Add(FIntVector(x, 0, z));
    return Out;
}
