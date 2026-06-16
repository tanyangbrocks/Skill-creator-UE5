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

    // ── 世界清單操作（對應 Godot FlowSaveSystem.Load / Delete）────────────
    // 掃描 ProjectSaved/Worlds/ 下所有子目錄，回傳有效 world.meta 的記錄
    static void ListAllWorlds(TArray<FWorldSaveData>& Out);

    // 建立新世界目錄 + 寫入 world.meta；Id 自動生成（GUID）
    static bool CreateNewWorld(const FString& Name, int32 Seed, FWorldSaveData& OutData);

    // 刪除整個世界目錄（不可逆）
    static bool DeleteWorld(const FString& WorldId);
};
