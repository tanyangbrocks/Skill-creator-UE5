#pragma once
#include "CoreMinimal.h"
#include "GridPos.h"
#include "MaterialType.h"
#include "WorldTypes.h"
#include "ICreature.h"

// 能力系統與世界之間的介面合約（對應 Godot IWorldInterface.cs §16）。
// 純 C++ 介面；AVoxelWorldActor + AEnemyManager 各自實作或透過 FWorldInterfaceAdapter 橋接。
// 使用場合：SpellCaster 等能力系統呼叫世界查詢/指令，不直接持有具體 Actor 指標。
class IWorldInterface
{
public:
    virtual ~IWorldInterface() = default;

    // ── 查詢 ────────────────────────────────────────────────────────────
    // 傳回佔據 Pos 格的實體；無實體則回傳 nullptr
    virtual ICreature*           GetEntityAt(FGridPos Pos)                          = 0;
    // 傳回 Pos 格的材質類型
    virtual EMaterialType        GetMaterialAt(FGridPos Pos)                        = 0;
    // 傳回所有在 Pos 曼哈頓距離 Radius 格以內的活體實體
    virtual TArray<ICreature*>   GetEntitiesNear(FGridPos Pos, float Radius)        = 0;

    // ── 指令 ────────────────────────────────────────────────────────────
    // 摧毀 Pos 格（設為 Air），觸發 OnTileDestroyed
    virtual void DestroyTile(FGridPos Pos,
                             EDestroyReason Reason = EDestroyReason::Mining)        = 0;
    // 在 XZ 平面向 Entity 施加位移力（單位：格/秒；負值代表反方向）
    virtual void ApplyForce(ICreature* Entity, float Dx, float Dz)                 = 0;
    // 生成效果（如火焰、水、煙霧）；Params 為浮點數 key-value（"Radius", "Duration" 等）
    virtual void SpawnEffect(FName EffectType, FGridPos Pos,
                             const TMap<FName, float>& Params)                      = 0;

    // ── 事件回呼（實作者在事件發生後呼叫；外部訂閱者綁 TFunction）───────
    TFunction<void(ICreature* /*Attacker*/, ICreature* /*Target*/, float /*Dmg*/)>
        OnEntityHit;
    TFunction<void(FGridPos /*Pos*/, EMaterialType /*Mat*/, EDestroyReason /*Reason*/)>
        OnTileDestroyed;
    TFunction<void(ICreature* /*Entity*/)>
        OnEntityDied;
    TFunction<void(FName /*Action*/, float /*Value*/)>
        OnPlayerAction;
};
