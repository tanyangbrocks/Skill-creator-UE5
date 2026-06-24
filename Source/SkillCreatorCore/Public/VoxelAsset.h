#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "MaterialType.h"
#include "VoxelAsset.generated.h"

// ── 最小儲存單元：.vox 一個體素轉換後的結果（plan-voxelization.md §6-1）
USTRUCT(BlueprintType)
struct SKILLCREATORCORE_API FVoxelCell
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) int16       X        = 0;  // 相對物件 pivot 的 tile 偏移
    UPROPERTY(EditAnywhere) int16       Y        = 0;
    UPROPERTY(EditAnywhere) int16       Z        = 0;
    UPROPERTY(EditAnywhere) EMaterialType Material = EMaterialType::Air;
};

// ── 調色盤映射 DataAsset：MagicaVoxel palette index → EMaterialType（§6-3）
// Editor 手動建立：Content/Data/DA_VoxelMaterialPalette.uasset
UCLASS(BlueprintType)
class SKILLCREATORCORE_API UVoxelMaterialPalette : public UDataAsset
{
    GENERATED_BODY()
public:
    // 長度 256；index 0 保留為 Air。index 1~255 對應 MagicaVoxel 調色盤格子
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<EMaterialType> PaletteToMaterial;

    EMaterialType Resolve(uint8 PaletteIndex) const;
};

// ── 體素資產 DataAsset：單一物件的所有非空體素（§6-2）
// 由 Editor Utility（V-2 實作）從 .vox 檔案匯入自動生成
UCLASS(BlueprintType)
class SKILLCREATORCORE_API UVoxelAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    // 所有非空 tile（Local 空間，相對物件 pivot）
    UPROPERTY(EditAnywhere)
    TArray<FVoxelCell> Cells;

    // 包圍盒（tile 單位，local 空間）
    UPROPERTY(EditAnywhere)
    FIntVector BoundsMin;

    UPROPERTY(EditAnywhere)
    FIntVector BoundsMax;

    // 對應的 Display Mesh（建立參照，方便 Editor 對照）
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UStaticMesh> SourceMesh;
};

// ── DataTable row：VoxelAssetId → UVoxelAsset（§6-4）
// RowName = FName asset id（e.g. "chest_oak", "tower_stone"）
// Editor 手動建立：Content/Data/DA_VoxelAssetRegistry.uasset
USTRUCT(BlueprintType)
struct SKILLCREATORCORE_API FVoxelAssetEntry : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelAsset> Asset;

    // 採用哪份調色盤（允許不同批素材使用不同規範）
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UVoxelMaterialPalette> Palette;

    // 注入時的 pivot 偏移（cm），用於校正美術原點與 tile 原點的差異
    UPROPERTY(EditAnywhere)
    FVector PivotOffset = FVector::ZeroVector;
};
