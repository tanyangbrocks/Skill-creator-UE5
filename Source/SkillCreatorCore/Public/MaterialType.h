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
    Air           = 0,
    Stone_Cobble  = 1,   // 圓石（石頭唯一當前子類型，舊 Stone ID 保留以對齊存檔）
    Dirt_Dry      = 2,   // 乾泥（泥土唯一當前子類型，舊 Dirt ID 保留以對齊存檔）
    Grass         = 3,
    Sand          = 4,
    Water         = 5,
    Lava          = 6,
    Wood          = 7,
    Leaves        = 8,
    Ore_Iron        =  9,
    Ore_Gold        = 10,
    Fire            = 11,  // Gas（CA 模擬）；CA_State = 倒計時 30-90 frames
    Steam           = 12,  // Gas（CA 模擬）；CA_State = 倒計時 60-120 frames
    Ash             = 13,  // Static（燃燒殘留）
    Ore_Coal        = 14,  // Static + Flammable（可燃礦石）
    Ore_Copper      = 15,  // Static
    Ore_MagicCrystal= 16,  // Static
    // 不可塑形可放置物的占位 tile（寶箱/工作臺等 PlacedFixtureActor 腳下格，純粹標記占用，
    // 不可採、不影響 Greedy Meshing 顯示——Actor 自己有 mesh，見 docs/plan-item-crafting-system.md §六）
    Fixture         = 17,
    FallenLeaf      = 18,  // 落葉 tile（可放置可塑形；EItemId::FallenLeaf 物品放回地面即此材質）
    // 繼續追加，不要重編已有 ID
    Count    UMETA(Hidden)
};

// 材質的「物理分類」（不同於 ETileCategory 的 gameplay 分類）：
// 工具類別（EToolCategory，ItemData.h）要跟這個比對才給 MiningSpeedMult 加成
// （docs/plan-item-crafting-system.md §四：木鏟只加速挖土/木斧只加速木製品/木鎬只加速石頭礦物）
UENUM(BlueprintType)
enum class EMaterialCategory : uint8
{
    None,
    Soil,
    Wood,
    Stone,
    Ore,
};

// Tile 資料單元：純 POD，背景 thread FArchive 序列化安全
// ⚠️ 禁止加 UObject 指標、FString、或任何需要 GC 的欄位
struct FTileCell
{
    uint8 MaterialID = 0;    // EMaterialType（查表取 UMaterialInstanceDynamic* 在 Game Thread）
    uint8 CA_State   = 0;    // CA 模擬狀態（流體、煙霧等）
    uint8 Category   = 0;    // ETileCategory（GameplayBlock / VisualCA）
    uint8 Flags      = 0;    // bit 0 = dirty, bit 1 = occupied, 其餘預留
    uint8 Variant    = 0;    // 0-255，微小色差用（Godot TileCell.cs:8）；生成時隨機指派
};
static_assert(sizeof(FTileCell) == 5, "FTileCell must be 5 bytes for cache efficiency");
