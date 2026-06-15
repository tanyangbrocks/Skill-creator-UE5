#include "FlowSaveSystem.h"
#include "CharacterSaveData.h"
#include "TileWorld3D.h"
#include "WorldSaveData.h"
#include "Misc/Paths.h"

FString FFlowSaveSystem::WorldRoot(const FString& WorldId)
{
    return FPaths::ProjectSavedDir() / TEXT("Worlds") / WorldId;
}

FString FFlowSaveSystem::MetaPath(const FString& WorldDir)
{
    return WorldDir / TEXT("world.meta");
}

bool FFlowSaveSystem::SaveAll(FTileWorld3D& World, const FWorldSaveData& WorldMeta,
                               const FCharacterSaveData& CharData)
{
    const FString& Dir = WorldMeta.WorldDir;
    if (Dir.IsEmpty()) return false;

    World.SaveDirtyChunks(Dir);
    WorldMeta.SaveMeta(MetaPath(Dir));
    CharData.Save(Dir);
    return true;
}

bool FFlowSaveSystem::LoadWorldMeta(const FString& WorldDir, FWorldSaveData& Out)
{
    return FWorldSaveData::LoadMeta(MetaPath(WorldDir), Out);
}

bool FFlowSaveSystem::LoadCharacter(const FString& WorldDir, FCharacterSaveData& Out)
{
    return FCharacterSaveData::Load(WorldDir, Out);
}
