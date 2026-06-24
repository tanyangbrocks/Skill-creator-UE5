#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MaterialType.h"
#include "VoxelAsset.generated.h"

// 最小儲存單元：.vox 一個體素轉換後的結果（plan-voxelization.md §6-1）
USTRUCT(BlueprintType)
struct SKILLCREATORCORE_API FVoxelCell
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) int16         X        = 0;
    UPROPERTY(EditAnywhere) int16         Y        = 0;
    UPROPERTY(EditAnywhere) int16         Z        = 0;
    UPROPERTY(EditAnywhere) EMaterialType Material = EMaterialType::Air;
};

// 體素資產（可選 DataAsset 快取；正式流程直接 runtime 讀 .vox 檔案，不需 Editor 手動建立）
// 僅在需要 Editor 預覽或預烘焙時使用
UCLASS(BlueprintType)
class SKILLCREATORCORE_API UVoxelAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere)
    TArray<FVoxelCell> Cells;

    UPROPERTY(EditAnywhere)
    FIntVector BoundsMin;

    UPROPERTY(EditAnywhere)
    FIntVector BoundsMax;

    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UStaticMesh> SourceMesh;
};
