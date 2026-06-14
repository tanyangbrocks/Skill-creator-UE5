#pragma once
#include "CoreMinimal.h"
#include "ManaType.generated.h"

// MP 三大根源分類（對應 Godot ManaType.RootGroup）
UENUM(BlueprintType)
enum class EManaRootGroup : uint8
{
    Cultivation  UMETA(DisplayName="修煉"),   // 武道/仙道/法道/意道/魂道/詭道
    Domination   UMETA(DisplayName="支配"),   // 魔法/妖力/奧術/神聖力/源能/星能
    World        UMETA(DisplayName="世界"),   // 技能/職業/超能/神力/概念/尋能
};

// 單一 MP 類型資料（對應 Godot ManaType record）
// W-6A：18 種基礎類型；W-13 追加複合類型（53 種）
USTRUCT(BlueprintType)
struct FManaType
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) int32          Id          = 0;
    UPROPERTY(EditAnywhere) FName          Key;              // e.g. "wu_dao"
    UPROPERTY(EditAnywhere) FText          DisplayName;      // e.g. "武道"
    UPROPERTY(EditAnywhere) EManaRootGroup RootGroup   = EManaRootGroup::Cultivation;
    UPROPERTY(EditAnywhere) bool           bIsComposite = false;
    UPROPERTY(EditAnywhere) int32          SortOrder   = 0;  // HUD 縱向排列順序
};

// 發動類型（對應 Godot AbilityActivationType.cs）
UENUM(BlueprintType)
enum class EAbilityActivationType : uint8
{
    Instant   UMETA(DisplayName="即時型"),    // ×0.8 MP，無宣告窗口
    Declare   UMETA(DisplayName="宣告型"),    // ×1.0 MP，有反應窗口
    Sustained UMETA(DisplayName="持續生效型"), // ×1.5 MP，持續消耗 MP
    None      UMETA(DisplayName="未指定"),    // ×1.0 MP
};
