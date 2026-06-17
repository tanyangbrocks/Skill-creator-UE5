#include "UDroppedItemManager.h"
#include "ADroppedItemActor.h"
#include "ASkillCreatorCharacter.h"
#include "MaterialRegistry.h"
#include "ItemDrop.h"
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

void UDroppedItemManager::SpawnForReason(int32 x, int32 y, int32 z,
                                          EMaterialType OldMat, EDestroyReason Reason)
{
    switch (Reason)
    {
    case EDestroyReason::Explosion:
        // 爆炸由技能邏輯另行生成碎片掉落物，此處跳過一般掉落避免重複
        return;
    case EDestroyReason::Mining:
    case EDestroyReason::ShapeMining:
    case EDestroyReason::Slash:
    case EDestroyReason::Crush:
        // 一般摧毀：依材質預設掉落表生成掉落物
        break;
    }

    FRandomStream Rng;
    Rng.Initialize(FMath::Rand());
    const FGridPos Pos(x, y, z);
    for (const FItemDrop& D : FMaterialRegistry::GetDefaultDrops(OldMat))
        if (D.ItemId != EItemId::None && Rng.FRand() <= D.Chance)
            SpawnDrop(D.ItemId, Rng.RandRange(D.MinCount, D.MaxCount), Pos);
}

void UDroppedItemManager::OnDropPickedUp(ADroppedItemActor* Drop)
{
    ActiveDrops.Remove(Drop);
}
