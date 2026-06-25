#pragma once
#include "CoreMinimal.h"
#include "CreatureTypes.generated.h"

// 所有「生物」的種類（新設計：敵人不是種類，而是 EHostility 標籤）
UENUM(BlueprintType)
enum class ECreatureKind : uint8
{
    Player         UMETA(DisplayName="玩家"),
    NPC            UMETA(DisplayName="NPC"),
    Beast          UMETA(DisplayName="獸"),       // 白獸/魔獸/靈獸/妖獸 → EBeastKind
    LifelikeEntity UMETA(DisplayName="類生命體"), // 預留：自動機、植物型怪物…
    Summon         UMETA(DisplayName="召喚物"),   // 預留：SpellRunner 臨時 Actor…
};

// 獸的四大子類（現有 AEnemy 戰鬥型態皆先歸入「魔獸」）
UENUM(BlueprintType)
enum class EBeastKind : uint8
{
    White  UMETA(DisplayName="白獸"),
    Magic  UMETA(DisplayName="魔獸"),
    Spirit UMETA(DisplayName="靈獸"),
    Demon  UMETA(DisplayName="妖獸"),
};

// 敵人標籤（可掛在任何 ECreatureKind 上，且可動態切換）
UENUM(BlueprintType)
enum class EHostility : uint8
{
    Neutral  UMETA(DisplayName="中立"),
    Friendly UMETA(DisplayName="友善"),
    Hostile  UMETA(DisplayName="敵對"),  // = 帶有「敵人」標籤
};
