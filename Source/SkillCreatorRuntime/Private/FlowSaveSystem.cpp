#include "FlowSaveSystem.h"
#include "CharacterSaveData.h"
#include "TileWorld3D.h"
#include "MapGenerator3D.h"
#include "WorldSaveData.h"
#include "WorldScale.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"

FString FFlowSaveSystem::WorldRoot(const FString& WorldId)
{
    return FPaths::ProjectSavedDir() / TEXT("Worlds") / WorldId;
}

FString FFlowSaveSystem::MetaPath(const FString& WorldDir)
{
    return WorldDir / TEXT("world.meta");
}

bool FFlowSaveSystem::SaveAll(FTileWorld3D& World, const FWorldSaveData& WorldMeta)
{
    const FString& Dir = WorldMeta.WorldDir;
    if (Dir.IsEmpty()) return false;

    World.SaveDirtyChunks(Dir);
    WorldMeta.SaveMeta(MetaPath(Dir));
    return true;
}

bool FFlowSaveSystem::LoadWorldMeta(const FString& WorldDir, FWorldSaveData& Out)
{
    return FWorldSaveData::LoadMeta(MetaPath(WorldDir), Out);
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

void FFlowSaveSystem::PregenerateSpawnArea(FTileWorld3D& World, FMapGenerator3D& Gen, FWorldSaveData& WorldMeta)
{
    StartPregenerateSpawnArea(World, Gen, WorldMeta);
    while (Gen.HasPendingChunks())
    {
        FPlatformProcess::Sleep(0.005f);
        Gen.ApplyPendingChunks(World, /*MaxPerFrame=*/999);
    }

    // 連通性後處理（FloodFill 打通出生區孤立洞穴）：所有 pending chunk 都 Apply 完後才能呼叫，
    // 範圍跟 StartPregenerateSpawnArea() 的 EnsureChunksAround Radius=2 一致。
    const FSpawnData Spawn = Gen.ComputeSpawnPoint(World);
    const int32 CCX = FMath::FloorToInt(static_cast<float>(Spawn.PlayerSpawn.X) / WorldScale::ChunkSize);
    const int32 CCY = FMath::FloorToInt(static_cast<float>(Spawn.PlayerSpawn.Y) / WorldScale::ChunkSize);
    const int32 CCZ = FMath::FloorToInt(static_cast<float>(Spawn.PlayerSpawn.Z) / WorldScale::ChunkSize);
    Gen.PostProcessRegion(World, FIntVector(CCX - 2, CCY - 2, CCZ - 2), FIntVector(CCX + 2, CCY + 2, CCZ + 2));

    FinishPregenerateSpawnArea(World, WorldMeta);
}

void FFlowSaveSystem::StartPregenerateSpawnArea(FTileWorld3D& World, FMapGenerator3D& Gen, FWorldSaveData& WorldMeta)
{
    const FSpawnData Spawn = Gen.ComputeSpawnPoint(World);

    const int32 CCX = FMath::FloorToInt(static_cast<float>(Spawn.PlayerSpawn.X) / WorldScale::ChunkSize);
    const int32 CCY = FMath::FloorToInt(static_cast<float>(Spawn.PlayerSpawn.Y) / WorldScale::ChunkSize);
    const int32 CCZ = FMath::FloorToInt(static_cast<float>(Spawn.PlayerSpawn.Z) / WorldScale::ChunkSize);

    UE_LOG(LogTemp, Log, TEXT("[FlowSave] StartPregenerate: WorldDir=%s Spawn=(%d,%d,%d) CenterChunk=(%d,%d,%d)"),
        *WorldMeta.WorldDir, Spawn.PlayerSpawn.X, Spawn.PlayerSpawn.Y, Spawn.PlayerSpawn.Z, CCX, CCY, CCZ);

    Gen.EnsureChunksAround(World, CCX, CCY, CCZ, /*Radius=*/2, /*MaxPerCall=*/999);

    WorldMeta.PlayerSpawn = Spawn.PlayerSpawn;

    UE_LOG(LogTemp, Log, TEXT("[FlowSave] StartPregenerate: launched %d chunk tasks"), Gen.GetPendingChunkCount());
}

void FFlowSaveSystem::FinishPregenerateSpawnArea(FTileWorld3D& World, FWorldSaveData& WorldMeta)
{
    int32 DirtyCount = 0;
    for (const auto& Pair : World.GetActiveChunks())
        if (Pair.Value && Pair.Value->bNeedsSave) ++DirtyCount;

    UE_LOG(LogTemp, Log, TEXT("[FlowSave] FinishPregenerate: WorldDir=%s ActiveChunks=%d DirtyChunks=%d"),
        *WorldMeta.WorldDir, World.GetActiveChunks().Num(), DirtyCount);

    World.SaveDirtyChunks(WorldMeta.WorldDir);

    WorldMeta.bIsFirstEnter = false;
    WorldMeta.SaveMeta(MetaPath(WorldMeta.WorldDir));

    UE_LOG(LogTemp, Log, TEXT("[FlowSave] FinishPregenerate: saved. PlayerSpawn=(%d,%d,%d)"),
        WorldMeta.PlayerSpawn.X, WorldMeta.PlayerSpawn.Y, WorldMeta.PlayerSpawn.Z);
}

void FFlowSaveSystem::ListAllCharacters(TArray<FCharacterSaveData>& Out)
{
    const FString CharactersRoot = FPaths::ProjectSavedDir() / TEXT("Characters");
    IFileManager::Get().IterateDirectory(*CharactersRoot,
        [&](const TCHAR* Path, bool bIsDirectory) -> bool
        {
            if (!bIsDirectory && FPaths::GetExtension(Path) == TEXT("json"))
            {
                const FString Id = FPaths::GetBaseFilename(Path);
                FCharacterSaveData Data;
                if (LoadCharacter(Id, Data))
                    Out.Add(Data);
            }
            return true;
        });
}

bool FFlowSaveSystem::CreateNewCharacter(const FString& Name, FCharacterSaveData& OutData)
{
    OutData = FCharacterSaveData();
    OutData.Id            = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    OutData.CharacterName = Name;
    return OutData.Save();
}

bool FFlowSaveSystem::CreateNewCharacter(const FCharacterSaveData& Prefilled, FCharacterSaveData& OutData)
{
    OutData    = Prefilled;
    OutData.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    return OutData.Save();
}

bool FFlowSaveSystem::LoadCharacter(const FString& CharacterId, FCharacterSaveData& Out)
{
    return FCharacterSaveData::Load(CharacterId, Out);
}

bool FFlowSaveSystem::SaveCharacter(const FCharacterSaveData& Character)
{
    return Character.Save();
}

bool FFlowSaveSystem::DeleteCharacter(const FString& CharacterId)
{
    const FString Path = FCharacterSaveData::FilePath(CharacterId);
    if (!IFileManager::Get().FileExists(*Path)) return false;
    return IFileManager::Get().Delete(*Path);
}
