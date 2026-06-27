#pragma once
#include "CoreMinimal.h"
#include "MaterialType.h"

// 地表裝飾物件定義（對應 docs/plan-surface-decoration.md DECO-0）
struct VOXELWORLD_API FDecorationDef
{
    FName            PropId;           // 唯一識別，用於 ISMC 分組
    FSoftObjectPath  MeshPath;         // e.g. /Game/Decorations/SM_Rock_Small.SM_Rock_Small
    TArray<EMaterialType> ValidSurfaces;  // 允許生長的地表材質
    float            Density     = 0.05f; // 0..1，每個符合格的生成機率
    FVector2D        ScaleRange  = FVector2D(0.8f, 1.2f);  // X=Min, Y=Max 均一縮放
    float            MaxSlopeDeg = 30.f; // 容許最大坡度（tile Y 差換算度）
    TArray<FName>    AllowedBiomes;    // 空 = 所有群系
};

// 純靜態 C++ 登錄表，日後新增裝飾物件只需在 Initialize() 加一行 Register()
class VOXELWORLD_API FDecorationRegistry
{
public:
    static void Initialize();
    static void Register(const FDecorationDef& Def);
    static const TArray<FDecorationDef>& GetAll();

private:
    static TArray<FDecorationDef> Defs;
    static bool bInitialized;
};
