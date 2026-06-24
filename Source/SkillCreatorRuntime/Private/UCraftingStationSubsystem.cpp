#include "UCraftingStationSubsystem.h"
#include "APlacedFixtureActor.h"
#include "WorldScale.h"

void UCraftingStationSubsystem::RegisterWorkbench(AWorkbenchActor* WB)
{
    if (WB) Workbenches.AddUnique(WB);
}

void UCraftingStationSubsystem::UnregisterWorkbench(AWorkbenchActor* WB)
{
    Workbenches.Remove(WB);
}

bool UCraftingStationSubsystem::HasNearbyWorkbench(const FVector& PlayerLoc, float RadiusGameUnits) const
{
    const float RadiusCm = RadiusGameUnits * static_cast<float>(WorldScale::GrainCurrent) * WorldScale::TileSizeCm;
    for (const TObjectPtr<AWorkbenchActor>& WB : Workbenches)
    {
        if (!WB) continue;
        if (FVector::DistSquared(WB->GetActorLocation(), PlayerLoc) <= RadiusCm * RadiusCm)
            return true;
    }
    return false;
}

void UCraftingStationSubsystem::RegisterChest(AChestActor* Chest)
{
    if (Chest) Chests.AddUnique(Chest);
}

void UCraftingStationSubsystem::UnregisterChest(AChestActor* Chest)
{
    Chests.Remove(Chest);
}

AChestActor* UCraftingStationSubsystem::FindNearbyChest(const FVector& PlayerLoc, float RadiusGameUnits) const
{
    const float RadiusCm = RadiusGameUnits * static_cast<float>(WorldScale::GrainCurrent) * WorldScale::TileSizeCm;
    for (const TObjectPtr<AChestActor>& Chest : Chests)
    {
        if (!Chest) continue;
        if (FVector::DistSquared(Chest->GetActorLocation(), PlayerLoc) <= RadiusCm * RadiusCm)
            return Chest;
    }
    return nullptr;
}

void UCraftingStationSubsystem::DestroyAllFixtures()
{
    // 先複製陣列再迭代：Destroy() 會同步觸發 EndPlay() → UnregisterWorkbench/UnregisterChest，
    // 邊迭代邊讓回呼修改原陣列（Workbenches/Chests）不安全。
    TArray<TObjectPtr<AWorkbenchActor>> WBCopy = Workbenches;
    for (const TObjectPtr<AWorkbenchActor>& WB : WBCopy)
        if (WB) WB->Destroy();

    TArray<TObjectPtr<AChestActor>> ChestCopy = Chests;
    for (const TObjectPtr<AChestActor>& Chest : ChestCopy)
        if (Chest) Chest->Destroy();

    Workbenches.Empty();
    Chests.Empty();
}
