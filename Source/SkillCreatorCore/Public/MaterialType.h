#pragma once
#include "CoreMinimal.h"
#include "MaterialType.generated.h"

// Tile 的 gameplay 分類（決定資料流向，M-10 雙軌制的基礎）
UENUM(BlueprintType)
enum class ETileCategory : uint8
{
    // 實心碰撞 tile（石頭/城牆/技能護盾）
    // → CPU authoritative，變更後即時推 GPU
    GameplayBlock   UMETA(DisplayName="Gameplay Block"),

    // 純視覺 CA 擴散（煙霧/裝飾水流）
    // → GPU 模擬，async readback 允許 ≤1 幀延遲
    VisualCA        UMETA(DisplayName="Visual CA"),
};

// 材質 ID（查表用；FTileCell.MaterialID 存的是此值）
UENUM(BlueprintType)
enum class EMaterialType : uint8
{
    Air      = 0,
    Stone    = 1,
    Dirt     = 2,
    Grass    = 3,
    Sand     = 4,
    Water    = 5,
    Lava     = 6,
    Wood     = 7,
    Leaves   = 8,
    Ore_Iron        =  9,
    Ore_Gold        = 10,
    Fire            = 11,  // Gas（CA 模擬）；CA_State = 倒計時 30-90 frames
    Steam           = 12,  // Gas（CA 模擬）；CA_State = 倒計時 60-120 frames
    Ash             = 13,  // Static（燃燒殘留）
    Ore_Coal        = 14,  // Static + Flammable（可燃礦石）
    Ore_Copper      = 15,  // Static
    Ore_MagicCrystal= 16,  // Static
    // 繼續追加，不要重編已有 ID
    Count    UMETA(Hidden)
};

// Tile 資料單元：純 POD，背景 thread FArchive 序列化安全
// ⚠️ 禁止加 UObject 指標、FString、或任何需要 GC 的欄位
struct FTileCell
{
    uint8 MaterialID = 0;    // EMaterialType（查表取 UMaterialInstanceDynamic* 在 Game Thread）
    uint8 CA_State   = 0;    // CA 模擬狀態（流體、煙霧等）
    uint8 Category   = 0;    // ETileCategory（GameplayBlock / VisualCA）
    uint8 Flags      = 0;    // bit 0 = dirty, bit 1 = occupied, 其餘預留
};
static_assert(sizeof(FTileCell) == 4, "FTileCell must be 4 bytes for cache efficiency");
