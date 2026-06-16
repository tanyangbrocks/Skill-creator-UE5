#include "FlowSaveSystem.h"
#include "CharacterSaveData.h"
#include "TileWorld3D.h"
#include "WorldSaveData.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"

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

void FFlowSaveSystem::ListAllWorlds(TArray<FWorldSaveData>& Out)
{
    const FString WorldsRoot = FPaths::ProjectSavedDir() / TEXT("Worlds");
    IFileManager::Get().IterateDirectory(*WorldsRoot,
        [&](const TCHAR* Path, bool bIsDirectory) -> bool
        {
            if (bIsDirectory)
            {
                FWorldSaveData Meta;
                if (LoadWorldMeta(Path, Meta))
                {
                    Meta.WorldDir = Path;
                    Out.Add(Meta);
                }
            }
            return true;
        });
}

bool FFlowSaveSystem::CreateNewWorld(const FString& Name, int32 Seed, FWorldSaveData& OutData)
{
    OutData.Id           = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    OutData.Name         = Name;
    OutData.Seed         = Seed;
    OutData.WorldDir     = WorldRoot(OutData.Id);
    OutData.bIsFirstEnter = true;
    OutData.PlayerSpawn  = FIntVector(0, 0, 0);

    IFileManager::Get().MakeDirectory(*OutData.WorldDir, true);
    return OutData.SaveMeta(MetaPath(OutData.WorldDir));
}

bool FFlowSaveSystem::DeleteWorld(const FString& WorldId)
{
    const FString Dir = WorldRoot(WorldId);
    if (!IFileManager::Get().DirectoryExists(*Dir)) return false;
    return IFileManager::Get().DeleteDirectory(*Dir, false, true);
}
