#include "UDroppedItemManager.h"
#include "ADroppedItemActor.h"
#include "ASkillCreatorCharacter.h"
#include "WorldScale.h"
#include "Engine/World.h"

ADroppedItemActor* UDroppedItemManager::SpawnDrop(EItemId ItemId, int32 Count, FGridPos WorldPos)
{
    UWorld* W = GetWorld();
    if (!W || ItemId == EItemId::None || Count <= 0) return nullptr;

    constexpr float TileSize = WorldScale::TileSizeCm;
    const FVector Location(
        WorldPos.X * TileSize,
        WorldPos.Z * TileSize,
        -WorldPos.Y * TileSize + TileSize * 0.5f
    );

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ADroppedItemActor* Drop = W->SpawnActor<ADroppedItemActor>(Location, FRotator::ZeroRotator, Params);
    if (!Drop) return nullptr;

    Drop->Init(ItemId, Count);
    Drop->OnPickedUp = [this](ADroppedItemActor* D){ OnDropPickedUp(D); };
    ActiveDrops.Add(Drop);
    return Drop;
}

TArray<FItemStack> UDroppedItemManager::TryPickupAll(ASkillCreatorCharacter* Player)
{
    TArray<FItemStack> Picked;
    if (!Player) return Picked;

    const FVector PPos = Player->GetActorLocation();
    for (int32 i = ActiveDrops.Num() - 1; i >= 0; --i)
    {
        ADroppedItemActor* Drop = ActiveDrops[i];
        if (!IsValid(Drop))
        {
            ActiveDrops.RemoveAt(i);
            continue;
        }
        if (Drop->CanPickup(PPos))
        {
            Picked.Add(FItemStack(Drop->ItemId, Drop->Count));
            ActiveDrops.RemoveAt(i);
            Drop->Destroy();
        }
    }
    return Picked;
}

void UDroppedItemManager::Clear()
{
    for (ADroppedItemActor* Drop : ActiveDrops)
        if (IsValid(Drop)) Drop->Destroy();
    ActiveDrops.Empty();
}

void UDroppedItemManager::OnDropPickedUp(ADroppedItemActor* Drop)
{
    ActiveDrops.Remove(Drop);
}
