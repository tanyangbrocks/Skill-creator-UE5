#pragma once
#include "CoreMinimal.h"
#include "MaterialType.h"
#include "ElementType.h"
#include "ItemId.h"
#include "ItemDrop.h"

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
    // ── CA 物理 ────────────────────────────────────────────────────────
    EPhysicsCategory  Physics       = EPhysicsCategory::Static;
    float             Density       = 0.f;    // 液體密度（高密度可置換低密度）
    bool              bFlammable    = false;  // 可被引燃（Static 燃燒用）
    uint8             BurnMin       = 0;      // CA_State 燃燒倒計時下限
    uint8             BurnMax       = 0;      // CA_State 燃燒倒計時上限
    ESkillElementType NativeElement = ESkillElementType::None; // CA 元素反應用

    // ── 採礦 / 遊戲機制（對應 Godot MaterialData.cs）──────────────────
    bool  bIsMineable       = false; // 可被採礦工具摧毀
    float Hardness          = 0.f;   // 硬度（0-5；影響採礦速度）
    int32 RequiredToolTier  = 0;     // 最低所需工具等級（0=徒手，1=基礎，2=鐵，3=高階）
    float BlastResistance   = 0.f;   // 爆炸傷害抵抗（每點減少等比傷害）
    float MagicResistance   = 0.f;   // 魔法傷害抵抗係數（0-1；1=完全免疫）
    uint8 Opacity           = 255;   // 透明度（255=完全不透明；0=完全透明）
    EItemId FragmentItem    = EItemId::None; // 採礦/爆炸碎裂後的主要掉落物 ID

    // H-6：是否需要半透明渲染（對應 Godot MaterialData.cs:51 IsTransparent）。
    // 刻意放在欄位表最後一個，現有 GMatData[] 的位置式初始化列表（13 個值）才不會
    // 因為插入新欄位而把後面的值全部錯位一格。
    bool  bIsTransparent    = false;

    // 2026-06-23 物品系統擴充：材質的物理分類，跟 ItemData.h 的 EToolCategory 比對才給
    // MiningSpeedMult 加成（docs/plan-item-crafting-system.md §四）。
    EMaterialCategory Category = EMaterialCategory::None;

    // 碎片系統脆度乘數（docs/plan-debris-fragment.md §四）。
    // 1.0 = 標準；> 1 = 易碎（水晶、石）；< 1 = 韌性（木、土、沙）。
    float Brittleness = 1.0f;
};

class VOXELWORLD_API FMaterialRegistry
{
public:
    static const FMaterialData&   Get(EMaterialType Mat);
    static EPhysicsCategory       GetPhysics(uint8 MaterialID);
    static EMaterialCategory      GetCategory(EMaterialType Mat);

    // ── 顯示用輔助（對應 Godot MaterialData 顯示欄位）────────────────
    // Variant：0-255 微小色差（對應 Godot MaterialRegistry.cs:83-92），預設 0 = 無偏移
    static FLinearColor           GetColor(EMaterialType Mat, uint8 Variant = 0);
    static FText                  GetDisplayName(EMaterialType Mat);
    static TArray<FItemDrop>      GetDefaultDrops(EMaterialType Mat);
    static EItemId                GetFragmentItem(EMaterialType Mat);
    static float                  GetBrittleness(EMaterialType Mat);
};
