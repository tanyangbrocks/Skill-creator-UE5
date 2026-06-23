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
class ADebrisActor;
class ASkillCreatorCharacter;

// 爆炸/斬擊/碾壓碎塊生成參數（docs/plan-debris-fragment.md §D-4）
struct FDebrisParams
{
    EDestroyReason Reason    = EDestroyReason::Explosion;
    float          Intensity = 1.f;    // 威力 0~1（技能強度換算）
    FVector        Direction = FVector::ZeroVector;  // 斬擊方向（Slash 用；ZeroVector=無方向）
};

// 世界掉落物管理器（對應 Godot DroppedItemManager.cs）。
// UWorldSubsystem 自動隨關卡建立 / 銷毀。
// SpawnDrop：靜態掉落物（採礦、箱子）。
// SpawnDebris：物理飛行碎塊（爆炸/斬擊/碾壓），計算後 spawn ADebrisActor。
// SpawnForReason：依摧毀原因路由。
// TryPickupAll：掃描玩家範圍內所有掉落物和碎塊，返回已拾取清單。
UCLASS()
class SKILLCREATORRUNTIME_API UDroppedItemManager : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    // 在 GridPos 位置生成靜態掉落物（採礦、箱子）
    ADroppedItemActor* SpawnDrop(EItemId ItemId, int32 Count, FGridPos WorldPos);

    // 依摧毀原因路由掉落：
    // Mining/ShapeMining → 材質預設掉落表（靜態掉落物）
    // Explosion → 跳過（由 OnExplodeComplete 聚合後呼叫 SpawnDebris）
    // Slash/Crush → SpawnDebris（單 tile 通常低於 threshold，不產碎塊）
    void SpawnForReason(int32 x, int32 y, int32 z, EMaterialType OldMat, EDestroyReason Reason);

    // 套用碎片公式 → spawn ADebrisActor + 給予衝量（docs/plan-debris-fragment.md §四）
    // tileCount < 13（1% of 1331）時不 spawn。
    void SpawnDebris(FIntVector Center, EMaterialType Mat, int32 TileCount,
                     const FDebrisParams& Params);

    // 嘗試拾取玩家周圍範圍內所有掉落物和碎塊；回傳成功拾取的 FItemStack 陣列
    TArray<FItemStack> TryPickupAll(ASkillCreatorCharacter* Player);

    // J-11（舊介面，保留相容）：直接生成 Fragment 靜態掉落物（無物理飛行）
    void SpawnFragments(FGridPos Center, EMaterialType Mat, int32 TileCount, EDestroyReason Reason);

    // 清除所有掉落物和碎塊
    void Clear();

    int32 GetDropCount()   const { return ActiveDrops.Num(); }
    int32 GetDebrisCount() const { return ActiveDebris.Num(); }

private:
    UPROPERTY()
    TArray<ADroppedItemActor*> ActiveDrops;

    UPROPERTY()
    TArray<ADebrisActor*> ActiveDebris;

    void OnDropPickedUp(ADroppedItemActor* Drop);
};
