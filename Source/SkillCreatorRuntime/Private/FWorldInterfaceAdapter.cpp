#include "FWorldInterfaceAdapter.h"
#include "AVoxelWorldActor.h"
#include "AEnemyManager.h"
#include "AEnemy.h"
#include "ASkillCreatorCharacter.h"
#include "TileWorld3D.h"
#include "EngineUtils.h"

ICreature* FWorldInterfaceAdapter::GetEntityAt(FGridPos Pos)
{
    if (EnemyMgr)
        for (AEnemy* E : EnemyMgr->GetEnemies())
            if (E && E->GetPosition() == Pos)
                return E;

    if (VoxelWorld)
        for (TActorIterator<ASkillCreatorCharacter> It(VoxelWorld->GetWorld()); It; ++It)
            if (It->GetPosition() == Pos)
                return *It;

    return nullptr;
}

EMaterialType FWorldInterfaceAdapter::GetMaterialAt(FGridPos Pos)
{
    FTileWorld3D* TW = VoxelWorld ? VoxelWorld->GetTileWorld() : nullptr;
    return TW ? TW->GetTile(Pos.X, Pos.Y, Pos.Z) : EMaterialType::Air;
}

TArray<ICreature*> FWorldInterfaceAdapter::GetEntitiesNear(FGridPos Pos, float Radius)
{
    TArray<ICreature*> Out;

    if (EnemyMgr)
        for (AEnemy* E : EnemyMgr->GetEnemies())
            if (E && E->IsAlive() && Pos.EuclideanDistance(E->GetPosition()) <= Radius)
                Out.Add(E);

    if (VoxelWorld)
        for (TActorIterator<ASkillCreatorCharacter> It(VoxelWorld->GetWorld()); It; ++It)
            if (Pos.EuclideanDistance(It->GetPosition()) <= Radius)
                Out.Add(*It);

    return Out;
}

FEntityPropertyValue FWorldInterfaceAdapter::GetEntityProperty(ICreature* Entity, FName Property)
{
    if (!Entity) return FEntityPropertyValue();
    if (Property == TEXT("hp"))    return FEntityPropertyValue::FromFloat(Entity->GetHp());
    if (Property == TEXT("maxHp")) return FEntityPropertyValue::FromFloat(Entity->GetMaxHp());
    // "faction" 等其他屬性目前沒有共通的資料來源（ICreature 沒有 faction 欄位），
    // 留給未來擴充——回傳 None 而不是猜一個值。
    return FEntityPropertyValue();
}

void FWorldInterfaceAdapter::DestroyTile(FGridPos Pos, EDestroyReason /*Reason*/)
{
    FTileWorld3D* TW = VoxelWorld ? VoxelWorld->GetTileWorld() : nullptr;
    if (TW) TW->SetTile(Pos.X, Pos.Y, Pos.Z, EMaterialType::Air);
}

void FWorldInterfaceAdapter::ApplyForce(ICreature* /*Entity*/, float /*Dx*/, float /*Dz*/)
{
    // 最小化實作：目前沒有任何呼叫端用到（NPC 感知系統只讀不寫），
    // ICreature 介面本身也沒有提供「施加位移」的方法可以呼叫，留待真正需要時再設計。
}

void FWorldInterfaceAdapter::SpawnEffect(FName /*EffectType*/, FGridPos /*Pos*/, const TMap<FName, float>& /*Params*/)
{
    // 最小化實作：目前沒有任何呼叫端用到，留待真正需要時再接上 Niagara/CA 效果系統。
}

void FWorldInterfaceAdapter::SetEntityProperty(ICreature* /*Entity*/, FName /*Property*/, FEntityPropertyValue /*Value*/)
{
    // 最小化實作：ICreature 介面本身是唯讀查詢介面（Get*() 系列），沒有對應的 setter
    // 可以呼叫；要支援寫入屬性需要先擴充 ICreature，留待真正需要時再設計。
}

ICreature* FWorldInterfaceAdapter::CreateEntity(FName /*Type*/, FGridPos /*Pos*/, const TMap<FName, FEntityPropertyValue>& /*Params*/)
{
    // 最小化實作：「依名稱生成任意實體」需要一個完整的工廠系統對應每種 Type 字串，
    // 目前沒有任何呼叫端需要它，先回傳 nullptr，避免猜測性設計。
    return nullptr;
}
