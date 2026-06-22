#pragma once
#include "CoreMinimal.h"
#include "RaceRegistry.generated.h"

// 血脈階級（對應 origin text setting/about w - 10 playercreate.txt 第 3 點：
// 天階 100 點 / 地階 50 點 / 玄階 10 點 / 黃階 0 點）
UENUM()
enum class ERaceTier : uint8
{
    Yellow,   // 黃階
    Profound, // 玄階
    Earth,    // 地階
    Heaven    // 天階
};

USTRUCT()
struct FRaceDefinition
{
    GENERATED_BODY()
    UPROPERTY() FName     Id;
    UPROPERTY() FName     SystemId;
    UPROPERTY() FText     DisplayName;
    UPROPERTY() ERaceTier Tier = ERaceTier::Yellow;
    UPROPERTY() FText     Description; // ≤100 字
};

USTRUCT()
struct FRaceSystemDefinition
{
    GENERATED_BODY()
    UPROPERTY() FName SystemId;
    UPROPERTY() FText DisplayName; // 體系圓球上顯示的名稱
};

// 種族資料表（對應 docs/plan-w10-character-creation.md §3.1）。純資料、無 gameplay 依賴，
// 放在 SkillCreatorCore（跟 FItemRegistry/FMaterialRegistry 同一個理由），UI 跟之後可能需要
// 種族加成的 AbilitySystem 都能直接依賴，不會有「低層 plugin 反過來依賴高層」的問題
// （2026-06-20 TotemLibrary 從 SkillCreatorUI 移到 AbilitySystem 的同一個教訓）。
//
// 資料來源：Import word setting/ExportBlock-game-word/ 下 16 份「體系」文件，逐一讀過後
// 整理（見計畫文件 §3.1.1）。「神界種族」依該文件內容判斷是飛升後狀態而非可創建種族，
// 故不在這份清單中註冊任何 Race，AllSystems() 也不列。
//
// 跟 FItemRegistry 同一套「首次呼叫時初始化，function-local static，不需要外部呼叫
// Initialize()」慣例（見 ItemRegistry.h/.cpp）。
struct SKILLCREATORCORE_API FRaceRegistry
{
    static const TArray<FRaceSystemDefinition>& AllSystems();
    static TArray<const FRaceDefinition*> RacesInSystem(FName SystemId);
    static const FRaceDefinition* Find(FName RaceId);

    // 黃0/玄10/地50/天100，對應使用者規格第 3 點
    static int32 CostForTier(ERaceTier Tier);

private:
    struct FData
    {
        TArray<FRaceSystemDefinition> Systems;
        TMap<FName, FRaceDefinition>  Races;
    };
    static void Init(TArray<FRaceSystemDefinition>& OutSystems, TMap<FName, FRaceDefinition>& OutRaces);
    static const FData& GetData();
};
