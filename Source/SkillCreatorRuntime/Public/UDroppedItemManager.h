#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ItemId.h"
#include "ItemStack.h"
#include "GridPos.h"
#include "WorldTypes.h"
#include "MaterialType.h"
#include "UDroppedItemManager.generated.h"

class ADroppedItemActor;
class ASkillCreatorCharacter;

// 世界掉落物管理器（對應 Godot DroppedItemManager.cs）。
// UWorldSubsystem 自動隨關卡建立 / 銷毀。
// SpawnDrop：在世界中生成掉落物 Actor。
// SpawnForReason：依摧毀原因路由掉落邏輯（爆炸/斬擊/採礦/碾壓/形狀採礦）。
// TryPickupAll：掃描玩家範圍內可拾取的掉落物，返回已拾取清單。
UCLASS()
class SKILLCREATORRUNTIME_API UDroppedItemManager : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    // 在 GridPos 位置生成掉落物（自動換算成世界座標；TileSize = 100 cm）
    ADroppedItemActor* SpawnDrop(EItemId ItemId, int32 Count, FGridPos WorldPos);

    // 依摧毀原因路由掉落：爆炸跳過一般掉落（由技能邏輯另行生成碎片）；
    // Mining / ShapeMining / Slash / Crush 使用材質預設掉落表。
    void SpawnForReason(int32 x, int32 y, int32 z, EMaterialType OldMat, EDestroyReason Reason);

    // 嘗試拾取玩家周圍範圍內所有掉落物；回傳成功拾取的 FItemStack 陣列
    TArray<FItemStack> TryPickupAll(ASkillCreatorCharacter* Player);

    // 清除所有掉落物
    void Clear();

    int32 GetDropCount() const { return ActiveDrops.Num(); }

private:
    UPROPERTY()
    TArray<ADroppedItemActor*> ActiveDrops;

    void OnDropPickedUp(ADroppedItemActor* Drop);
};
