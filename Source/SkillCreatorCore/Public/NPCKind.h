#pragma once
#include "CoreMinimal.h"
#include "NPCKind.generated.h"

// NPC 分類架構（docs/plan-base-npc-system.md §二）。純資料表，不含行為邏輯，只負責
// 「這個分類存在、屬於哪個大類、目前有沒有實作」。
UENUM(BlueprintType)
enum class ENPCMacroCategory : uint8
{
    Special   UMETA(DisplayName="特殊NPC"),   // 特殊設計+特殊功能，暫不實作任何具體個體
    Regional  UMETA(DisplayName="區域NPC"),   // 限定特定區域，暫不實作任何具體個體
    General   UMETA(DisplayName="通用NPC"),   // 種族/勢力/身份隨機，僅用「性質」分類
};

UENUM(BlueprintType)
enum class ENPCGeneralKind : uint8
{
    TravelingMerchant UMETA(DisplayName="旅行商人"),
    Traveler          UMETA(DisplayName="旅行者"),
    Transcendent      UMETA(DisplayName="超界執行者"),
    Criminal          UMETA(DisplayName="罪犯"),
    Bard              UMETA(DisplayName="吟遊詩人"),
    Villager          UMETA(DisplayName="村民"),
    Refugee           UMETA(DisplayName="逃難者"),
};

USTRUCT(BlueprintType)
struct FNPCSubtypeDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName           Id;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ENPCGeneralKind Kind = ENPCGeneralKind::Bard;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText           DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText           Description;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool            bImplemented = false;
};

// 對應 FRaceRegistry/FItemRegistry 同一套「靜態登錄表」慣例
struct SKILLCREATORCORE_API FNPCKindRegistry
{
    static const TArray<FNPCSubtypeDefinition>& AllSubtypes();
    static const FNPCSubtypeDefinition* Find(FName SubtypeId);
private:
    static void Init(TArray<FNPCSubtypeDefinition>& Out);
};
