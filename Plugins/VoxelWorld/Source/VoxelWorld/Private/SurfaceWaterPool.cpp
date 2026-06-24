#include "SurfaceWaterPool.h"
#include "WorldScale.h"

void FSurfaceWaterPool::Initialize(int32 Seed, int32 WorldWidth, int32 WorldHeight, int32 WorldDepth)
{
    WorldW = WorldWidth;
    WorldH = WorldHeight;
    WorldD = WorldDepth;
    Pools.Empty();

    FRandomStream Rng;
    Rng.Initialize(Seed ^ 0x5F3C2B1);
    const int32 Count = Rng.RandRange(CountMin, CountMax - 1);
    const int32 G     = WorldScale::GrainCurrent;

    const bool bInfiniteX = WorldW <= 0;
    const bool bInfiniteZ = WorldD <= 0;
    const int32 CenterX = bInfiniteX ? 0 : WorldW / 2;
    const int32 CenterZ = bInfiniteZ ? 0 : WorldD / 2;

    // 保底：池 0 距出生點至少 (radius+64) tiles，讓玩家走幾步就能看到（對應 Godot Initialize() 46-58 行）
    const int32 BaseRadius = Rng.RandRange(4, 6) * G;
    const int32 Dist0      = BaseRadius + 64;
    int32 SpawnCX, SpawnCZ;
    switch (Rng.RandRange(0, 3))
    {
        case 0:  SpawnCX = CenterX + Dist0; SpawnCZ = CenterZ;        break;
        case 1:  SpawnCX = CenterX - Dist0; SpawnCZ = CenterZ;        break;
        case 2:  SpawnCX = CenterX;         SpawnCZ = CenterZ + Dist0; break;
        default: SpawnCX = CenterX;         SpawnCZ = CenterZ - Dist0; break;
    }
    if (!bInfiniteX) SpawnCX = FMath::Clamp(SpawnCX, EdgeMargin, FMath::Max(EdgeMargin, WorldW - EdgeMargin));
    if (!bInfiniteZ) SpawnCZ = FMath::Clamp(SpawnCZ, EdgeMargin, FMath::Max(EdgeMargin, WorldD - EdgeMargin));

    FPoolDesc P0;
    P0.CX        = SpawnCX;
    P0.CZ        = SpawnCZ;
    P0.Radius    = BaseRadius;
    P0.MaxDepth  = Rng.RandRange(DepthMin, DepthMax - 1);
    Pools.Add(P0);

    for (int32 i = 1; i < Count; ++i)
    {
        FPoolDesc P;
        P.CX = bInfiniteX ? (CenterX + Rng.RandRange(-ScatterRadius, ScatterRadius))
                           : Rng.RandRange(EdgeMargin, FMath::Max(EdgeMargin, WorldW - EdgeMargin));
        P.CZ = bInfiniteZ ? (CenterZ + Rng.RandRange(-ScatterRadius, ScatterRadius))
                           : Rng.RandRange(EdgeMargin, FMath::Max(EdgeMargin, WorldD - EdgeMargin));
        P.Radius   = Rng.RandRange(RadiusMin, RadiusMax - 1) * G;
        P.MaxDepth = Rng.RandRange(DepthMin, DepthMax - 1);
        Pools.Add(P);
    }
}

void FSurfaceWaterPool::Prepare(const TFunction<int32(int32, int32)>& GetHeight)
{
    // 以各池中心的自然高度固定水面 Y（對應 Godot Prepare() 70-75 行）。
    // UE5 Y 增大 = 向下，故水面比中心地表「加深」(1-WaterFill) 比例（Godot Y-up 為「減」）。
    for (FPoolDesc& P : Pools)
    {
        const int32 CenterH = GetHeight(P.CX, P.CZ);
        P.WaterSurfaceY = CenterH + FMath::RoundToInt(P.MaxDepth * (1.f - WaterFill));
    }
}

bool FSurfaceWaterPool::QueryOverride(const TArray<FPoolDesc>& Pools, int32 WorldX, int32 WorldZ,
                                       int32 NaturalSurfaceY, int32& OutEffectiveY, EMaterialType& OutMat)
{
    // 對應 Godot GetSurfaceOverride() + BowlSurface()，座標方向已換算成 UE5 的 Y-down 慣例。
    for (const FPoolDesc& P : Pools)
    {
        if (P.Radius <= 0) continue;
        const int64 Dx = (int64)WorldX - P.CX;
        const int64 Dz = (int64)WorldZ - P.CZ;
        const int64 Dist2 = Dx * Dx + Dz * Dz;
        if (Dist2 > (int64)P.Radius * P.Radius) continue;

        const float T         = FMath::Sqrt((float)Dist2) / (float)P.Radius;  // 0=中心 1=邊緣
        const int32 BowlDepth = FMath::RoundToInt(P.MaxDepth * (1.f - T * T)); // 拋物面深度
        if (BowlDepth <= 0) continue;  // 邊緣外，不影響

        const int32 FloorY = NaturalSurfaceY + BowlDepth;  // 挖深 = Y 加大（向下）
        if (FloorY >= P.WaterSurfaceY)
        {
            // 碗底已經深過水面 → 此處是水下
            OutEffectiveY = P.WaterSurfaceY;
            OutMat        = EMaterialType::Water;
        }
        else
        {
            // 碗緣還沒到水面 → 露出的凹陷土岸
            OutEffectiveY = FloorY;
            OutMat        = EMaterialType::Dirt_Dry;
        }
        return true;
    }
    return false;
}
