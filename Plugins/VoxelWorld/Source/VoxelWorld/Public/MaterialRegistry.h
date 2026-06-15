#pragma once
#include "CoreMinimal.h"
#include "MaterialType.h"
#include "ElementType.h"

// CA 物理分類（決定每 tick 的更新行為）
enum class EPhysicsCategory : uint8
{
    Empty   = 0,  // Air — 不模擬
    Static  = 1,  // Stone/Dirt/Wood/Ash — 靜止（可能可燃）
    Powder  = 2,  // Sand — 受重力落下
    Liquid  = 3,  // Water/Lava — 受重力 + 橫向擴散
    Gas     = 4,  // Fire/Steam — 上浮 + 倒計時消散
};

struct FMaterialData
{
    EPhysicsCategory Physics     = EPhysicsCategory::Static;
    float            Density     = 0.f;                    // 液體密度（高密度可置換低密度）
    bool             bFlammable  = false;                  // 可被引燃（Static 燃燒用）
    uint8            BurnMin     = 0;                      // CA_State 燃燒倒計時下限
    uint8            BurnMax     = 0;                      // CA_State 燃燒倒計時上限
    ESkillElementType NativeElement = ESkillElementType::None; // 材質對應元素（CA 元素反應用）
};

class VOXELWORLD_API FMaterialRegistry
{
public:
    static const FMaterialData& Get(EMaterialType Mat);
    static EPhysicsCategory     GetPhysics(uint8 MaterialID);
};
