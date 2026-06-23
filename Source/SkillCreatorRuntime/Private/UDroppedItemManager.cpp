#include "UDroppedItemManager.h"
#include "ADroppedItemActor.h"
#include "ADebrisActor.h"
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

    // 靜態掉落物
    for (int32 i = ActiveDrops.Num() - 1; i >= 0; --i)
    {
        ADroppedItemActor* Drop = ActiveDrops[i];
        if (!IsValid(Drop)) { ActiveDrops.RemoveAt(i); continue; }
        if (Drop->CanPickup(PPos))
        {
            Picked.Add(FItemStack(Drop->ItemId, Drop->Count));
            ActiveDrops.RemoveAt(i);
            Drop->Destroy();
        }
    }

    // 物理碎塊
    for (int32 i = ActiveDebris.Num() - 1; i >= 0; --i)
    {
        ADebrisActor* D = ActiveDebris[i];
        if (!IsValid(D)) { ActiveDebris.RemoveAt(i); continue; }
        if (D->CanPickup(PPos))
        {
            if (D->FragmentCount > 0 && D->FragmentItemId != EItemId::None)
                Picked.Add(FItemStack(D->FragmentItemId, D->FragmentCount));
            ActiveDebris.RemoveAt(i);
            D->Destroy();
        }
    }

    return Picked;
}

void UDroppedItemManager::Clear()
{
    for (ADroppedItemActor* Drop : ActiveDrops)
        if (IsValid(Drop)) Drop->Destroy();
    ActiveDrops.Empty();

    for (ADebrisActor* D : ActiveDebris)
        if (IsValid(D)) D->Destroy();
    ActiveDebris.Empty();
}

void UDroppedItemManager::SpawnForReason(int32 x, int32 y, int32 z,
                                          EMaterialType OldMat, EDestroyReason Reason)
{
    switch (Reason)
    {
    case EDestroyReason::Explosion:
    case EDestroyReason::ShapeMining:
        // Explosion：由 OnExplodeComplete 聚合後呼叫 SpawnDebris，per-tile 跳過
        // ShapeMining：由呼叫端批次處理
        return;

    case EDestroyReason::Slash:
    case EDestroyReason::Crush:
    {
        // 走 SpawnDebris；單 tile (count=1) 低於 threshold=13，通常不產碎塊
        // 移除舊有的錯誤行為（slash 不應給方塊掉落）
        FDebrisParams P;
        P.Reason    = Reason;
        P.Intensity = 1.f;
        SpawnDebris(FIntVector(x, y, z), OldMat, 1, P);
        return;
    }

    case EDestroyReason::Mining:
        break;  // 走下方材質預設掉落表
    }

    FRandomStream Rng;
    Rng.Initialize(FMath::Rand());
    const FGridPos Pos(x, y, z);
    for (const FItemDrop& D : FMaterialRegistry::GetDefaultDrops(OldMat))
        if (D.ItemId != EItemId::None && Rng.FRand() <= D.Chance)
            SpawnDrop(D.ItemId, Rng.RandRange(D.MinCount, D.MaxCount), Pos);
}

void UDroppedItemManager::SpawnDebris(FIntVector Center, EMaterialType Mat,
                                       int32 TileCount, const FDebrisParams& Params)
{
    // 碎片公式：docs/plan-debris-fragment.md §四
    constexpr int32 TilesPerUnit  = 1331;  // 11³ = DefaultShapeRadius=5 Cube
    constexpr int32 ThresholdTiles = 13;   // 1% of 1331

    if (TileCount < ThresholdTiles) return;

    EItemId FragId = FMaterialRegistry::GetFragmentItem(Mat);
    if (FragId == EItemId::None) return;

    const float Brittleness = FMaterialRegistry::GetBrittleness(Mat);
    const float Units       = (float)TileCount / TilesPerUnit;

    float BaseRate   = 0.f;
    float AngleFactor = 1.f;
    switch (Params.Reason)
    {
    case EDestroyReason::Mining:
    case EDestroyReason::ShapeMining:
        BaseRate = 100.f;
        break;
    case EDestroyReason::Explosion:
        BaseRate = FMath::RandRange(20.f, 80.f);
        break;
    case EDestroyReason::Slash:
        BaseRate = FMath::RandRange(30.f, 70.f);
        if (!Params.Direction.IsNearlyZero())
        {
            // 垂直入射（CosAngle≈1）→ 切乾淨=0.7；斜角（CosAngle≈0）→ 碎更多=1.3
            const float CosAngle = FMath::Abs(
                FVector::DotProduct(Params.Direction.GetSafeNormal(), FVector::UpVector));
            AngleFactor = FMath::Lerp(1.3f, 0.7f, CosAngle);
        }
        break;
    case EDestroyReason::Crush:
        BaseRate = FMath::RandRange(50.f, 100.f);
        break;
    }

    const int32 Count = FMath::RoundToInt(Units * BaseRate * Params.Intensity * Brittleness * AngleFactor);
    if (Count <= 0) return;

    UWorld* W = GetWorld();
    if (!W) return;

    int32 WorldH = WorldScale::DefaultWorldHeight;
    if (AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(W))
        WorldH = VW->WorldHeight;

    const FVector WorldPos = WorldScale::TileToWorld(FGridPos(Center.X, Center.Y, Center.Z), WorldH);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ADebrisActor* Debris = W->SpawnActor<ADebrisActor>(WorldPos, FRotator::ZeroRotator, SpawnParams);
    if (!Debris) return;

    Debris->Init(FragId, Count);

    // 衝量方向：有明確方向（斬擊/碾壓）→ 反彈帶上揚；無方向（爆炸）→ 隨機外擴
    FVector ImpulseDir;
    if (!Params.Direction.IsNearlyZero())
        ImpulseDir = (-Params.Direction + FVector(0.f, 0.f, 0.5f)).GetSafeNormal();
    else
        ImpulseDir = FVector(FMath::RandRange(-1.f, 1.f),
                             FMath::RandRange(-1.f, 1.f),
                             FMath::RandRange(0.3f, 1.f)).GetSafeNormal();

    Debris->LaunchImpulse(ImpulseDir * (400.f * Params.Intensity));

    ActiveDebris.Add(Debris);
}

void UDroppedItemManager::OnDropPickedUp(ADroppedItemActor* Drop)
{
    ActiveDrops.Remove(Drop);
}

void UDroppedItemManager::SpawnFragments(FGridPos Center, EMaterialType Mat,
                                          int32 TileCount, EDestroyReason Reason)
{
    // J-11（舊介面）：生成靜態 Fragment 掉落物（無物理飛行），保留相容
    EItemId FragId = FMaterialRegistry::GetFragmentItem(Mat);
    if (FragId == EItemId::None) return;

    const float Units = (float)TileCount / 1331.f;  // 1 unit = 11³ = 1331 tiles
    int32 Count = 0;
    if (Reason == EDestroyReason::Mining || Reason == EDestroyReason::ShapeMining)
        Count = FMath::RoundToInt(Units * 100.f);
    else if (Reason == EDestroyReason::Explosion)
        Count = FMath::RoundToInt(Units * FMath::RandRange(20.f, 80.f));

    if (Count > 0)
        SpawnDrop(FragId, Count, Center);
}
