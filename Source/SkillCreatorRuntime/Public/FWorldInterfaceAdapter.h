#pragma once
#include "CoreMinimal.h"
#include "IWorldInterface.h"

class AVoxelWorldActor;
class AEnemyManager;

// 橋接 AVoxelWorldActor + AEnemyManager 實作 IWorldInterface（docs/plan-base-npc-system.md
// §四，解開 NPCBrain plugin M-NPC-3 留下的遺留缺口：UNPCPerceptionComponent 一直都依賴這個
// 介面，但專案裡從未有任何地方真正實作過）。
//
// 目前只有 GetEntitiesNear()/GetMaterialAt() 被 UNPCPerceptionComponent 實際呼叫到（已對照
// 原始碼確認），其餘介面方法給出正確但最小化的實作（不是空殼，但功能範疇刻意收斂，避免
// 超出本次計畫範圍——例如 CreateEntity 的「依名稱生成任意實體」需要一個完整的工廠系統，
// 目前沒有任何呼叫端需要它，先回傳 nullptr）。
//
// 寫好這個 Adapter 後，敵人/未來其它生物的感知能力也能一起受益，不是 NPC 專屬的接線。
class SKILLCREATORRUNTIME_API FWorldInterfaceAdapter : public IWorldInterface
{
public:
    FWorldInterfaceAdapter(AVoxelWorldActor* InVoxelWorld, AEnemyManager* InEnemyMgr)
        : VoxelWorld(InVoxelWorld), EnemyMgr(InEnemyMgr) {}

    virtual ICreature*           GetEntityAt(FGridPos Pos) override;
    virtual EMaterialType        GetMaterialAt(FGridPos Pos) override;
    virtual TArray<ICreature*>   GetEntitiesNear(FGridPos Pos, float Radius) override;
    virtual FEntityPropertyValue GetEntityProperty(ICreature* Entity, FName Property) override;

    virtual void DestroyTile(FGridPos Pos, EDestroyReason Reason = EDestroyReason::Mining) override;
    virtual void ApplyForce(ICreature* Entity, float Dx, float Dz) override;
    virtual void SpawnEffect(FName EffectType, FGridPos Pos, const TMap<FName, float>& Params) override;
    virtual void SetEntityProperty(ICreature* Entity, FName Property, FEntityPropertyValue Value) override;
    virtual ICreature* CreateEntity(FName Type, FGridPos Pos, const TMap<FName, FEntityPropertyValue>& Params) override;

private:
    AVoxelWorldActor* VoxelWorld = nullptr;
    AEnemyManager*    EnemyMgr   = nullptr;
};
