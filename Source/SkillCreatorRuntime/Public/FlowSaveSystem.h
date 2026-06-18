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

    // Save: dirty chunks (binary) + world meta (text). Character data is
    // independent of any world (see character roster section below) and is
    // saved separately via SaveCharacter().
    // WorldMeta.WorldDir must be set to the target save directory.
    static bool SaveAll(FTileWorld3D& World, const FWorldSaveData& WorldMeta);

    // Load world meta from disk (chunks load lazily as player moves).
    static bool LoadWorldMeta(const FString& WorldDir, FWorldSaveData& Out);

    // Meta file path: {WorldDir}/world.meta
    static FString MetaPath(const FString& WorldDir);

    // ── 世界清單操作（對應 Godot FlowSaveSystem.Load / Delete）────────────
    // 掃描 ProjectSaved/Worlds/ 下所有子目錄，回傳有效 world.meta 的記錄
    static void ListAllWorlds(TArray<FWorldSaveData>& Out);

    // 建立新世界目錄 + 寫入 world.meta；Id 自動生成（GUID）
    static bool CreateNewWorld(const FString& Name, int32 Seed, FWorldSaveData& OutData);

    // 刪除整個世界目錄（不可逆）
    static bool DeleteWorld(const FString& WorldId);

    // ── 角色清單操作（對應 Godot 全域 _chars 清單，獨立於世界）─────────────
    // 掃描 ProjectSaved/Characters/ 下所有 .json，回傳所有角色存檔
    static void ListAllCharacters(TArray<FCharacterSaveData>& Out);

    // 建立新角色（僅名稱，其餘走 FCharacterSaveData 預設值）；Id 自動生成（GUID）
    static bool CreateNewCharacter(const FString& Name, FCharacterSaveData& OutData);

    // 載入指定角色存檔
    static bool LoadCharacter(const FString& CharacterId, FCharacterSaveData& Out);

    // 寫回角色目前進度（Character.Id 決定存檔位置）
    static bool SaveCharacter(const FCharacterSaveData& Character);

    // 刪除角色存檔（不可逆）
    static bool DeleteCharacter(const FString& CharacterId);
};
