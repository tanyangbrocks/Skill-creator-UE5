#pragma once
#include "CoreMinimal.h"
#include "WorldHierarchy.generated.h"

// W-F：世界層級定義（純設計資料，暫不實作生成邏輯）
// 來源：docs/plan-more-world-setting.md §一~四

// 大區域（8 大陸 + 海）
UENUM(BlueprintType)
enum class EWorldRegion : uint8
{
    None            = 0,
    Tianhui         = 1,    // 天輝大陸
    Ruize           = 2,    // 瑞澤大陸
    Xuanxiao        = 3,    // 璇霄大陸
    DreamSorrow     = 4,    // 夢之殤
    DemonArchipelago= 5,    // 魔域群島
    DemonPeak       = 6,    // 萬魔之巔
    ShadowContinent = 7,    // 幽影大陸
    EndlessSea      = 8,    // 無盡海地區
};

// 生物群系類型（15 種，對應 plan §二）
UENUM(BlueprintType)
enum class EBiomeType : uint8
{
    None            = 0,
    Civilization    = 1,    // 文明生物群系（城鎮）
    CherryForest    = 2,    // 櫻花木森林
    OakForest       = 3,    // 橡木森林
    BerryJungle     = 4,    // 漿果叢林
    GreenGrassland  = 5,    // 青綠草原
    WildGrassland   = 6,    // 蒼莽草原
    YellowDesert    = 7,    // 黃土沙漠
    PeakMountain    = 8,    // 尖鋒山地
    TropicalOcean   = 9,    // 熱帶海洋
    FlatSnowfield   = 10,   // 平坦雪原
    Swampland       = 11,   // 沼澤濕地
    Badlands        = 12,   // 紅土惡地
    Underground     = 13,   // 礦坑地下
    MagneticCavern  = 14,   // 磁場地穴
    AncientDepths   = 15,   // 遠古深層地穴
    WorldCore       = 16,   // 一號世界核心
    LavaNether      = 17,   // 熔岩地獄
};

// 地形/大結構類型（對應 plan §三）
UENUM(BlueprintType)
enum class ETerrainFeature : uint8
{
    None            = 0,
    CrystalCave     = 1,    // 水晶洞窟
    VerdantCave     = 2,    // 蒼鬱洞窟
    SpiderDen       = 3,    // 蜘蛛洞穴
    StalactiteCave  = 4,    // 鐘乳石洞穴
    LavaLake        = 5,    // 岩漿湖
    GhostOutpost    = 6,    // 鬼族據點
    GoblinOutpost   = 7,    // 哥布林據點
};

// 地貌/小結構類型（對應 plan §四）
UENUM(BlueprintType)
enum class ELandform : uint8
{
    None            = 0,
    WaterPool       = 1,    // 水池（現有 FSurfaceWaterPool 實作）
    Cottage         = 2,    // 小屋
    UndergroundRail = 3,    // 地下軌道
    AncientRuins    = 4,    // 舊址
    UndergroundRiver= 5,    // 地下河川
    AncientPortal   = 6,    // 遠古傳送陣（指定位置，不由演算法生成）
};
