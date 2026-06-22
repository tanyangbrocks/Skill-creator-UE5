#pragma once
#include "CoreMinimal.h"

class FTileWorld3D;
class FMapGenerator3D;
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

    // 對應 Godot GameFlowUI.PregenerateWorld（GameFlowUI.cs:487-502）：
    // 計算出生點、預生成出生點周圍 chunk（Radius=2）、存檔、寫回 PlayerSpawn + bIsFirstEnter=false。
    // World/Gen 由呼叫端提供 —— UGameFlowWidget 建立世界時用獨立 standalone 物件（無 AVoxelWorldActor），
    // ASkillCreatorGameMode 首次進入世界時用 AVoxelWorldActor 既有的 TileWorld/MapGenerator
    // （兩處邏輯完全相同，曾各自重複一份，現抽成共用函式）。
    static void PregenerateSpawnArea(FTileWorld3D& World, FMapGenerator3D& Gen, FWorldSaveData& WorldMeta);

    // PregenerateSpawnArea 的非阻塞版本拆成兩半：StartPregenerateSpawnArea 算出生點 +
    // 送出 EnsureChunksAround（立即回傳，不等待），呼叫端自行逐 tick 呼叫
    // Gen.ApplyPendingChunks() + 檢查 Gen.HasPendingChunks()，全部完成後呼叫
    // FinishPregenerateSpawnArea 存檔。PregenerateSpawnArea 本身（同步 busy-wait）
    // 保留給 ASkillCreatorGameMode 的防呆路徑用（理論上很少觸發，阻塞可接受）；
    // UGameFlowWidget 建立世界的 UI 互動路徑改用這兩個，避免整個視窗在生成期間被
    // Sleep-loop 凍結到 OS 認為「沒有回應」（2026-06-21 使用者實測回報卡住，
    // 詳見 docs/開發血汗錄.md）。
    static void StartPregenerateSpawnArea(FTileWorld3D& World, FMapGenerator3D& Gen, FWorldSaveData& WorldMeta);
    static void FinishPregenerateSpawnArea(FTileWorld3D& World, FWorldSaveData& WorldMeta);

    // ── 角色清單操作（對應 Godot 全域 _chars 清單，獨立於世界）─────────────
    // 掃描 ProjectSaved/Characters/ 下所有 .json，回傳所有角色存檔
    static void ListAllCharacters(TArray<FCharacterSaveData>& Out);

    // 建立新角色（僅名稱，其餘走 FCharacterSaveData 預設值）；Id 自動生成（GUID）
    static bool CreateNewCharacter(const FString& Name, FCharacterSaveData& OutData);

    // W-10：建立新角色，但用呼叫端已經填好的 FCharacterSaveData（角色創建精靈跑完 RaceId/
    // BasePoint_* 等欄位後呼叫這個，Id 仍自動生成覆蓋掉呼叫端可能填的舊值）
    static bool CreateNewCharacter(const FCharacterSaveData& Prefilled, FCharacterSaveData& OutData);

    // 載入指定角色存檔
    static bool LoadCharacter(const FString& CharacterId, FCharacterSaveData& Out);

    // 寫回角色目前進度（Character.Id 決定存檔位置）
    static bool SaveCharacter(const FCharacterSaveData& Character);

    // 刪除角色存檔（不可逆）
    static bool DeleteCharacter(const FString& CharacterId);
};
