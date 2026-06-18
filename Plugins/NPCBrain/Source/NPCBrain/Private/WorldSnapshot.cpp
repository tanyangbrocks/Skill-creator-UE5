#include "WorldSnapshot.h"

bool FNPCPerceptionLogic::IsHazardMaterial(EMaterialType Mat)
{
	switch (Mat)
	{
		case EMaterialType::Lava:
		case EMaterialType::Fire:
		case EMaterialType::Steam:
			return true;
		default:
			return false;
	}
}

FString FNPCPerceptionLogic::MaterialDisplayName(EMaterialType Mat)
{
	switch (Mat)
	{
		case EMaterialType::Lava:  return TEXT("熔岩");
		case EMaterialType::Fire:  return TEXT("火焰");
		case EMaterialType::Steam: return TEXT("蒸氣");
		default:                  return TEXT("未知危險物質");
	}
}

TArray<FString> FNPCPerceptionLogic::DescribeChanges(const FWorldSnapshot& Prev, const FWorldSnapshot& Current)
{
	TArray<FString> Out;

	int32 NewlyAppeared = 0;
	for (int32 Id : Current.NearbyCreatureIds)
		if (!Prev.NearbyCreatureIds.Contains(Id)) ++NewlyAppeared;

	int32 NewlyGone = 0;
	for (int32 Id : Prev.NearbyCreatureIds)
		if (!Current.NearbyCreatureIds.Contains(Id)) ++NewlyGone;

	if (NewlyAppeared > 0)
		Out.Add(FString::Printf(TEXT("察覺到 %d 個新接近的生物"), NewlyAppeared));
	if (NewlyGone > 0)
		Out.Add(FString::Printf(TEXT("%d 個原本在附近的生物已不再出現"), NewlyGone));

	for (EMaterialType Mat : Current.HazardMaterials)
		if (!Prev.HazardMaterials.Contains(Mat))
			Out.Add(FString::Printf(TEXT("附近出現了%s"), *MaterialDisplayName(Mat)));

	for (EMaterialType Mat : Prev.HazardMaterials)
		if (!Current.HazardMaterials.Contains(Mat))
			Out.Add(FString::Printf(TEXT("附近的%s已經消退"), *MaterialDisplayName(Mat)));

	return Out;
}
