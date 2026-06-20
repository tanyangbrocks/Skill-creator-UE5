#include "UDroppedItemManager.h"
#include "ADroppedItemActor.h"
#include "ASkillCreatorCharacter.h"
#include "MaterialRegistry.h"
#include "ItemDrop.h"
#include "WorldScale.h"
#include "AVoxelWorldActor.h"
#include "Engine/World.h"

ADroppedItemActor* UDroppedItemManager::SpawnDrop(EItemId ItemId, int32 Count, FGridPos WorldPos)
{
    UWorld* W = GetWorld();
    if (!W || ItemId == EItemId::None || Count <= 0) return nullptr;

    int32 WorldH = WorldScale::DefaultWorldHeight;
    if (AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(W))
        WorldH = VW->WorldHeight;
    const FVector Location = WorldScale::TileToWorld(WorldPos, WorldH);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ADroppedItemActor* Drop = W->SpawnActor<ADroppedItemActor>(Location, FRotator::ZeroRotator, Params);
    if (!Drop) return nullptr;

    Drop->Init(ItemId, Count);
    Drop->OnPickedUp = [this](ADroppedItemActor* D){ OnDropPickedUp(D); };
    // J-12：注入世界參照供重力 Tick 使用
    Drop->CachedVoxelWorld = AVoxelWorldActor::FindInWorld(W);
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
    case EDestroyReason::ShapeMining:
        // 爆炸/形狀採掘：由呼叫端（ASkillCreatorCharacter::OnMine）批次處理掉落，
        // per-tile 不生成（對應 Godot DroppedItemManager.cs:15-16 Spawn() 的早退判斷）
        return;
    case EDestroyReason::Mining:
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

void UDroppedItemManager::SpawnFragments(FGridPos Center, EMaterialType Mat,
                                          int32 TileCount, EDestroyReason Reason)
{
    // J-11：對應 Godot DroppedItemManager.cs:31-46
    EItemId FragId = FMaterialRegistry::GetFragmentItem(Mat);
    if (FragId == EItemId::None) return;

    const float Units = (float)TileCount / 1000.f;
    int32 Count = 0;
    if (Reason == EDestroyReason::Mining || Reason == EDestroyReason::ShapeMining)
        Count = FMath::RoundToInt(Units * 100.f);   // Mining=100 碎片/unit（完整回收）
    else if (Reason == EDestroyReason::Explosion)
        Count = FMath::RoundToInt(Units * FMath::RandRange(20.f, 80.f));  // Explosion=20~80/unit

    if (Count > 0)
        SpawnDrop(FragId, Count, Center);
}
