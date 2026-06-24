#include "UMobMeshRegistry.h"

UDataTable* FMobMeshRegistry::GetTable()
{
    static UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_MobMeshRegistry.DT_MobMeshRegistry"));
    return Table;
}

FMobMeshResolved FMobMeshRegistry::Find(FName Key)
{
    FMobMeshResolved Out;
    UDataTable* Table = GetTable();
    if (!Table) return Out; // 資產還沒建立，呼叫端 fallback

    const FMobMeshEntry* Row = Table->FindRow<FMobMeshEntry>(Key, TEXT("FMobMeshRegistry::Find"));
    if (!Row) return Out;

    Out.Mesh        = Row->Mesh.LoadSynchronous();
    Out.Material    = Row->Material.LoadSynchronous();
    Out.AnimBPClass = Row->AnimBPClass.LoadSynchronous();
    return Out;
}
