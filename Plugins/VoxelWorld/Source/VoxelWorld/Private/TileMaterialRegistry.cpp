#include "TileMaterialRegistry.h"

UMaterialInterface* UTileMaterialRegistry::GetSurface(EMaterialType Type) const
{
	const int32 Idx = static_cast<int32>(Type);
	if (!Entries.IsValidIndex(Idx)) return nullptr;
	return Entries[Idx].SurfaceMaterial.Get();
}

UMaterialInterface* UTileMaterialRegistry::GetEmissive(EMaterialType Type) const
{
	const int32 Idx = static_cast<int32>(Type);
	if (!Entries.IsValidIndex(Idx)) return nullptr;
	return Entries[Idx].EmissiveMaterial.Get();
}

bool UTileMaterialRegistry::Validate(TArray<FString>& OutErrors) const
{
	bool bOk = true;
	const int32 ExpectedCount = static_cast<int32>(EMaterialType::Count);

	if (Entries.Num() != ExpectedCount)
	{
		OutErrors.Add(FString::Printf(
			TEXT("UTileMaterialRegistry: Entries 數量 %d，應為 %d（EMaterialType::Count）"),
			Entries.Num(), ExpectedCount));
		bOk = false;
	}

	for (int32 i = 1; i < ExpectedCount; ++i) // i=0 是 Air，不需要材質
	{
		if (!Entries.IsValidIndex(i) || !Entries[i].IsValid())
		{
			OutErrors.Add(FString::Printf(
				TEXT("EMaterialType[%d] 缺少 SurfaceMaterial"), i));
			bOk = false;
		}
	}
	return bOk;
}
