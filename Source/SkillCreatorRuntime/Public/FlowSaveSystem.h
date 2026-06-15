#pragma once
#include "CoreMinimal.h"

class FTileWorld3D;
struct FWorldSaveData;
struct FCharacterSaveData;

// Coordinates world chunk saves, world meta, and character JSON.
// All methods are static — no instance state.
class SKILLCREATORRUNTIME_API FFlowSaveSystem
{
public:
    // Root path for all world saves: {ProjectSaved}/Worlds/{WorldId}/
    static FString WorldRoot(const FString& WorldId);

    // Save: dirty chunks (binary) + world meta (text) + character (JSON).
    // WorldMeta.WorldDir must be set to the target save directory.
    static bool SaveAll(FTileWorld3D& World, const FWorldSaveData& WorldMeta,
                        const FCharacterSaveData& CharData);

    // Load world meta from disk (chunks load lazily as player moves).
    static bool LoadWorldMeta(const FString& WorldDir, FWorldSaveData& Out);

    // Load character data from disk.
    static bool LoadCharacter(const FString& WorldDir, FCharacterSaveData& Out);

    // Meta file path: {WorldDir}/world.meta
    static FString MetaPath(const FString& WorldDir);
};
