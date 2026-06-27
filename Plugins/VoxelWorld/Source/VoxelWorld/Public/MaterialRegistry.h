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

// 接觸狀態效果（P-12；uint8 enum 避免 FName 靜態初始化問題）
// 站在 / 浸入材質格時施加給角色的即時效果
enum class EContactEffect : uint8
{
    None        = 0,
    Burning     = 1,  // 持續火焰傷害
    Wet         = 2,  // 濕潤（防火）
    Poison      = 3,  // 中毒
    Electric    = 4,  // 感電
    Frozen      = 5,  // 冰凍
    Radioactive = 6,  // 輻射
};

struct FMaterialData
{
    // ── CA 物理 ────────────────────────────────────────────────────────
    EPhysicsCategory  Physics       = EPhysicsCategory::Static;
    // 液體/粉末 CA 排擠密度（高密度可置換低密度；Sand=6.0f 為 CA 比例尺，非現實密度）
    // Static/Gas 材質使用現實相對密度（水=1.0），僅用於物理計算（衝擊力/浮沉），不影響 CA
    float             Density       = 0.f;
    bool              bFlammable    = false;  // 可被引燃（Static 燃燒用）
    uint8             BurnMin       = 0;      // CA_State 燃燒倒計時下限
    uint8             BurnMax       = 0;      // CA_State 燃燒倒計時上限
    ESkillElementType NativeElement = ESkillElementType::None; // CA 元素反應用

    // ── 採礦 / 遊戲機制（對應 Godot MaterialData.cs）──────────────────
    bool  bIsMineable       = false;
    float Hardness          = 0.f;   // 0-5；影響採礦速度
    int32 RequiredToolTier  = 0;     // 0=徒手，1=基礎，2=鐵，3=高階
    float BlastResistance   = 0.f;   // 爆炸傷害抵抗
    float MagicResistance   = 0.f;   // 魔法傷害抵抗（0-1）
    uint8 Opacity           = 255;   // 255=完全不透明；0=透明
    EItemId FragmentItem    = EItemId::None;

    // H-6：半透明渲染旗標
    bool  bIsTransparent    = false;

    // 2026-06-23 物品系統擴充
    EMaterialCategory Category = EMaterialCategory::None;

    // 碎片系統脆度（1.0=標準；>1=易碎；<1=韌性）
    float Brittleness = 1.0f;

    // ── Plan-MP 新增屬性（docs/plan-material-physics.md）─────────────
    // 🔴 高優先（P-1 ~ P-5）
    float          AutoignitionTemp      = -1.f;               // P-1: 自燃溫度(°C)；-1=不自燃
    EMaterialType  MeltToMaterial        = EMaterialType::Air; // P-2: 加熱轉換；Air=無
    EMaterialType  FreezeToMaterial      = EMaterialType::Air; // P-3: 冷凍轉換；Air=無
    float          ElectricalConductivity = 0.f;               // P-4: 導電度(0-1)
    uint8          LuminanceLevel        = 0;                  // P-5: 自發光等級(0-15)

    // 🟡 中優先（P-6 ~ P-11）
    float          LiquidFlowSpeed       = 1.0f;               // P-6: 液體流速(0-1)；僅Liquid有效
    float          LiquidViscosity       = 0.f;                // P-7: 液體黏度(0-1)
    float          GasUpwardSpeed        = 1.0f;               // P-8: 氣體上浮係數(0-2)；僅Gas有效
    float          GasHorizontalSpeed    = 1.0f;               // P-9: 氣體橫向擴散係數(0-2)
    uint16         GasLifetime           = 0;                  // P-10: 氣體壽命(tick)；0=靠BurnMax
    EMaterialType  BreakToMaterial       = EMaterialType::Air; // P-11: 破壞留存材質；Air=清空

    // 🟢 低優先（P-12 ~ P-19）
    EContactEffect ContactEffect         = EContactEffect::None; // P-12: 接觸狀態效果
    float          SpeedFactor           = 1.0f;               // P-13: 站立移速乘數(1=正常)
    float          Stickyness            = 0.f;                // P-14: 浸入液體阻尼(0-1)
    float          Slippery              = 0.f;                // P-15: 表面滑度(0=正常；1=完全滑)
    float          Restitution           = 0.f;                // P-16: 落地彈性係數(0-1)
    float          JumpFactor            = 1.0f;               // P-17: 跳躍高度乘數(1=正常)
    uint8          PlatformType          = 0;                  // P-18: 0=實心；1=可穿越；2=單向
    uint8          DangerFlags           = 0;                  // P-19: bit0=火 bit1=水 bit2=毒 bit3=輻射
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

    // PHYS-1：查 /Game/PhysicalMaterials/PM_{MatName} 資產的軟參照路徑
    // 供 PHYS-2 GreedyMesher 為 RMC section 指定 Physical Material，
    // 以及 PHYS-3 APhysicalItemActor 在攜帶/落地時查詢 Restitution
    static FSoftObjectPath        GetPhysicalMaterialPath(EMaterialType Mat);
};
