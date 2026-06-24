#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Materials/MaterialInterface.h"
#include "MaterialType.h"
#include "TileMaterialRegistry.generated.h"

// 單一材質種類的渲染素材資料
USTRUCT(BlueprintType)
struct VOXELWORLD_API FTileMaterialEntry
{
	GENERATED_BODY()

	// Greedy Mesh 用的主表面材質
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UMaterialInterface> SurfaceMaterial;

	// 可選：自發光材質（岩漿、發光礦石等）
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UMaterialInterface> EmissiveMaterial;

	bool IsValid() const { return !SurfaceMaterial.IsNull(); }
};

// Tile 渲染材質登記表（UDataAsset）
// Entries 陣列 index = (uint8)EMaterialType → O(1) 查表
// 在 Editor 建立 DA_TileMaterialRegistry.uasset 並填寫每個 Tile 種類的材質。
UCLASS(BlueprintType)
class VOXELWORLD_API UTileMaterialRegistry : public UDataAsset
{
	GENERATED_BODY()
public:
	// 長度應等於 (int32)EMaterialType::Count（共 19 格：ID 0-18 + FallenLeaf）
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FTileMaterialEntry> Entries;

	// 取已同步載入的 Surface 材質；未載入或未設定回傳 nullptr
	UMaterialInterface* GetSurface(EMaterialType Type) const;

	// 取已同步載入的 Emissive 材質；未設定回傳 nullptr
	UMaterialInterface* GetEmissive(EMaterialType Type) const;

	// 驗證所有非 Air 的 Entry 是否都填了 SurfaceMaterial（Editor Preflight 用）
	bool Validate(TArray<FString>& OutErrors) const;
};
