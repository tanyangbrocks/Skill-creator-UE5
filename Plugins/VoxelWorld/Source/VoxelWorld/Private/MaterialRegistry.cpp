#include "MaterialRegistry.h"

using E  = ESkillElementType;
using P  = EPhysicsCategory;
using I  = EItemId;
using M  = EMaterialType;
using EC = EContactEffect;

// 材質資料表（與 EMaterialType 一一對應，依 ID 順序）
// 欄位順序（前 16）：Physics, Density, bFlammable, BurnMin, BurnMax, NativeElement,
//   bIsMineable, Hardness, RequiredToolTier, BlastResistance, MagicResistance, Opacity,
//   FragmentItem, bIsTransparent, Category, Brittleness
// 欄位順序（Plan-MP 新增 17-35）：
//   AutoignitionTemp, MeltToMaterial, FreezeToMaterial, ElectricalConductivity, LuminanceLevel,
//   LiquidFlowSpeed, LiquidViscosity, GasUpwardSpeed, GasHorizontalSpeed, GasLifetime,
//   BreakToMaterial, ContactEffect, SpeedFactor, Stickyness, Slippery,
//   Restitution, JumpFactor, PlatformType, DangerFlags
// 省略尾端欄位 = 使用 struct 預設值
// Density 說明：Sand=6.0f 為 CA 排擠比例尺（非現實密度）；其餘材質使用現實相對密度（水=1.0）
static const FMaterialData GMatData[] =
{
    // ID  0 — Air（全預設）
    { P::Empty,  0.f,  false, 0,   0,   E::None,  false, 0.f,  0, 0.f,  0.f,  0,   I::None },

    // ID  1 — Stone_Cobble（圓石）
    // Density 2.5（現實花崗岩）；ElecCond 0.05（微導電）；Restitution 0.02
    { P::Static, 2.5f, false, 0,   0,   E::Earth, true,  3.f,  1, 2.f,  0.5f, 255, I::FragmentStone, false, EMaterialCategory::Stone, 1.2f,
      -1.f, M::Air, M::Air, 0.05f },

    // ID  2 — Dirt_Dry（乾泥）
    // Density 1.3；Restitution 0.02（些微緩衝）
    { P::Static, 1.3f, false, 0,   0,   E::Earth, true,  1.f,  0, 0.5f, 0.f,  255, I::FragmentDirt,  false, EMaterialCategory::Soil,  0.5f,
      -1.f, M::Air, M::Air, 0.f, 0, 1.0f, 0.f, 1.0f, 1.0f, 0, M::Air, EC::None, 1.0f, 0.f, 0.f, 0.02f },

    // ID  3 — Grass
    // Density 1.0；AutoIgn 180°C；Restitution 0.05（草皮緩衝）
    { P::Static, 1.0f, false, 0,   0,   E::Wood,  true,  1.f,  0, 0.5f, 0.f,  255, I::FragmentDirt,  false, EMaterialCategory::Soil,  0.5f,
      180.f, M::Air, M::Air, 0.f, 0, 1.0f, 0.f, 1.0f, 1.0f, 0, M::Air, EC::None, 1.0f, 0.f, 0.f, 0.05f },

    // ID  4 — Sand（Godot MaterialRegistry.cs:31 Density=6.0f > Water → 沙沉入水成立；保持 CA 比例尺）
    { P::Powder, 6.0f, false, 0,   0,   E::Earth, true,  0.5f, 0, 0.2f, 0.f,  255, I::FragmentSand,  false, EMaterialCategory::Soil,  0.4f },

    // ID  5 — Water（H-6：Opacity 140 + bIsTransparent；Godot MaterialRegistry.cs:16-17）
    // ElecCond 0.8；FreezeToMaterial=Air（Ice 材質待新增）；Stickyness 0.3
    { P::Liquid, 1.0f, false, 0,   0,   E::Water, false, 0.f,  0, 0.f,  0.2f, 140, I::None,          true,  EMaterialCategory::None,  1.0f,
      -1.f, M::Air, M::Air, 0.8f, 0, 1.0f, 0.f, 1.0f, 1.0f, 0, M::Air, EC::None, 1.0f, 0.3f },

    // ID  6 — Lava（H-6：Godot 未標 IsTransparent，完全不透明；Godot MaterialRegistry.cs:18-19）
    // FreezeToMaterial=Stone_Cobble；LuminanceLevel=15；LiquidFlowSpeed=0.333（Godot TileWorld3D.cs:265 每3幀）
    // LiquidViscosity=0.9；ContactEffect=Burning；Stickyness=0.95；DangerFlags=bit0(Fire)
    { P::Liquid, 3.0f, false, 0,   0,   E::Fire,  false, 0.f,  0, 0.f,  0.5f, 255, I::None,          false, EMaterialCategory::None,  1.0f,
      -1.f, M::Air, M::Stone_Cobble, 0.f, 15,
      0.333f, 0.9f, 1.0f, 1.0f, 0, M::Air,
      EC::Burning, 1.0f, 0.95f, 0.f, 0.f, 1.0f, 0, 0b0001 },

    // ID  7 — Wood（H-5：Godot RequiredToolTier=0 徒手砍樹）
    // Density 0.6（松木）；AutoIgn 250°C；ElecCond 0.01
    { P::Static, 0.6f, true,  80,  200, E::Wood,  true,  1.5f, 0, 0.5f, 0.f,  255, I::FragmentWood,  false, EMaterialCategory::Wood,  0.7f,
      250.f, M::Air, M::Air, 0.01f },

    // ID  8 — Leaves
    // Density 0.3；AutoIgn 180°C
    { P::Static, 0.3f, true,  20,  60,  E::Wood,  true,  0.5f, 0, 0.2f, 0.f,  180, I::FragmentWood,  false, EMaterialCategory::Wood,  0.6f,
      180.f },

    // ID  9 — Ore_Iron
    // Density 7.9（純鐵）；ElecCond 1.0
    { P::Static, 7.9f, false, 0,   0,   E::Metal, true,  4.f,  2, 3.f,  1.f,  255, I::OreIronRaw,    false, EMaterialCategory::Ore,   1.1f,
      -1.f, M::Air, M::Air, 1.0f },

    // ID 10 — Ore_Gold
    // Density 19.3（純金）；ElecCond 0.8
    { P::Static, 19.3f,false, 0,   0,   E::Metal, true,  3.5f, 2, 2.5f, 0.5f, 255, I::OreGoldRaw,    false, EMaterialCategory::Ore,   1.1f,
      -1.f, M::Air, M::Air, 0.8f },

    // ID 11 — Fire（H-6：Opacity 166 + bIsTransparent；Godot MaterialRegistry.cs:20-21）
    // Density 0.001（近似無質量）；LuminanceLevel=13；GasUpward=1.5；GasHorizontal=0.8；DangerFlags=bit0
    { P::Gas,  0.001f, false, 0,   0,   E::Fire,  false, 0.f,  0, 0.f,  0.5f, 166, I::None,          true,  EMaterialCategory::None,  1.0f,
      -1.f, M::Air, M::Air, 0.f, 13,
      1.0f, 0.f, 1.5f, 0.8f, 0, M::Air,
      EC::None, 1.0f, 0.f, 0.f, 0.f, 1.0f, 0, 0b0001 },

    // ID 12 — Steam（H-6：Opacity 89 + bIsTransparent；Godot MaterialRegistry.cs:22-23）
    // Density 0.0006（水蒸氣）；GasUpward=2.0；GasHorizontal=0.6；GasLifetime=240 tick
    { P::Gas, 0.0006f, false, 0,   0,   E::Water, false, 0.f,  0, 0.f,  0.f,   89, I::None,          true,  EMaterialCategory::None,  1.0f,
      -1.f, M::Air, M::Air, 0.f, 0,
      1.0f, 0.f, 2.0f, 0.6f, 240 },

    // ID 13 — Ash
    // Density 0.3（木灰）；無新欄位非預設值
    { P::Static, 0.3f, false, 0,   0,   E::None,  true,  0.3f, 0, 0.1f, 0.f,  255, I::FragmentAsh,   false, EMaterialCategory::None,  0.3f },

    // ID 14 — Ore_Coal（H-5：Hardness 1.5f）
    // Density 1.4；AutoIgn 400°C
    { P::Static, 1.4f, true,  180, 240, E::Earth, true,  1.5f, 1, 2.f,  0.5f, 255, I::OreCoal,       false, EMaterialCategory::Ore,   1.0f,
      400.f },

    // ID 15 — Ore_Copper
    // Density 8.9（純銅）；ElecCond 1.0
    { P::Static, 8.9f, false, 0,   0,   E::Metal, true,  3.5f, 1, 2.5f, 0.5f, 255, I::OreCopperRaw,  false, EMaterialCategory::Ore,   1.1f,
      -1.f, M::Air, M::Air, 1.0f },

    // ID 16 — Ore_MagicCrystal（H-5：RequiredToolTier=2）
    // Density 2.8；LuminanceLevel=8；Restitution=0.3（彈跳感）
    { P::Static, 2.8f, false, 0,   0,   E::Light, true,  4.5f, 2, 2.f,  3.f,  210, I::OreMagicCrystal,false,EMaterialCategory::Ore,  1.5f,
      -1.f, M::Air, M::Air, 0.f, 8,
      1.0f, 0.f, 1.0f, 1.0f, 0, M::Air,
      EC::None, 1.0f, 0.f, 0.f, 0.3f },

    // ID 17 — Fixture（不可塑形可放置物腳下占位 tile）
    { P::Empty,  0.f,  false, 0,   0,   E::None,  false, 0.f,  0, 0.f,  0.f,  255, I::None },

    // ID 18 — FallenLeaf（落葉 tile）
    // Density 0.2；AutoIgn 180°C
    { P::Static, 0.2f, true,  10,  30,  E::Wood,  true,  0.2f, 0, 0.1f, 0.f,  210, I::FallenLeaf,    false, EMaterialCategory::Wood,  0.6f,
      180.f },
};

static constexpr uint8 GMatDataCount = (uint8)(sizeof(GMatData) / sizeof(GMatData[0]));

const FMaterialData& FMaterialRegistry::Get(EMaterialType Mat)
{
    uint8 ID = (uint8)Mat;
    if (ID >= GMatDataCount)
    {
        static const FMaterialData Fallback{};
        return Fallback;
    }
    return GMatData[ID];
}

EPhysicsCategory FMaterialRegistry::GetPhysics(uint8 MaterialID)
{
    if (MaterialID >= GMatDataCount) return EPhysicsCategory::Empty;
    return GMatData[MaterialID].Physics;
}

EMaterialCategory FMaterialRegistry::GetCategory(EMaterialType Mat)
{
    uint8 ID = (uint8)Mat;
    if (ID >= GMatDataCount) return EMaterialCategory::None;
    return GMatData[ID].Category;
}

// ── 顯示顏色查找表（RGBA 0-1）────────────────────────────────────────────
static const FLinearColor GMatColors[] =
{
    FLinearColor(0.f,   0.f,   0.f,   0.f),    // 0: Air (transparent)
    FLinearColor(0.50f, 0.50f, 0.50f, 1.f),    // 1: Stone (grey)
    FLinearColor(0.50f, 0.30f, 0.10f, 1.f),    // 2: Dirt (brown)
    FLinearColor(0.20f, 0.70f, 0.20f, 1.f),    // 3: Grass (green)
    FLinearColor(0.80f, 0.70f, 0.30f, 1.f),    // 4: Sand (yellow)
    FLinearColor(0.10f, 0.40f, 0.90f, 0.78f),  // 5: Water (blue, semi-transparent)
    FLinearColor(1.00f, 0.30f, 0.10f, 0.86f),  // 6: Lava (orange-red)
    FLinearColor(0.40f, 0.20f, 0.05f, 1.f),    // 7: Wood (dark brown)
    FLinearColor(0.15f, 0.50f, 0.10f, 0.70f),  // 8: Leaves (green, semi-transparent)
    FLinearColor(0.60f, 0.40f, 0.30f, 1.f),    // 9: Ore_Iron (iron-tinted stone)
    FLinearColor(0.85f, 0.70f, 0.10f, 1.f),    // 10: Ore_Gold (gold)
    FLinearColor(1.00f, 0.50f, 0.10f, 0.59f),  // 11: Fire (orange, semi-transparent)
    FLinearColor(0.70f, 0.70f, 0.70f, 0.39f),  // 12: Steam (light grey, transparent)
    FLinearColor(0.30f, 0.30f, 0.30f, 1.f),    // 13: Ash (dark grey)
    FLinearColor(0.20f, 0.20f, 0.20f, 1.f),    // 14: Ore_Coal (very dark)
    FLinearColor(0.60f, 0.40f, 0.20f, 1.f),    // 15: Ore_Copper (copper)
    FLinearColor(0.60f, 0.40f, 1.00f, 0.82f),  // 16: Ore_MagicCrystal (purple)
    FLinearColor(0.40f, 0.40f, 0.45f, 1.f),    // 17: Fixture (slate grey，通常被 Actor mesh 蓋住看不到)
    FLinearColor(0.55f, 0.38f, 0.12f, 0.82f),  // 18: FallenLeaf (橘褐，半透明)
};

FLinearColor FMaterialRegistry::GetColor(EMaterialType Mat, uint8 Variant)
{
    uint8 ID = (uint8)Mat;
    if (ID >= GMatDataCount) return FLinearColor::White;
    const FLinearColor& C = GMatColors[ID];

    // 對應 Godot MaterialRegistry.cs:83-92：v = (variant/255)*0.06 - 0.03，僅偏移 RGB，Alpha 不變
    const float V = (Variant / 255.f) * 0.06f - 0.03f;
    return FLinearColor(
        FMath::Clamp(C.R + V, 0.f, 1.f),
        FMath::Clamp(C.G + V, 0.f, 1.f),
        FMath::Clamp(C.B + V, 0.f, 1.f),
        C.A);
}

// ── 顯示名稱查找表（中文）────────────────────────────────────────────────
static const TCHAR* const GMatNames[] =
{
    TEXT("空氣"),     TEXT("圓石"),   TEXT("乾泥"),   TEXT("草地"),   TEXT("沙"),
    TEXT("水"),       TEXT("熔岩"),   TEXT("木頭"),   TEXT("樹葉"),   TEXT("鐵礦石"),
    TEXT("金礦石"),   TEXT("火焰"),   TEXT("蒸汽"),   TEXT("灰燼"),   TEXT("煤礦"),
    TEXT("銅礦石"),   TEXT("魔法水晶"), TEXT("固定物"), TEXT("落葉"),
};

FText FMaterialRegistry::GetDisplayName(EMaterialType Mat)
{
    uint8 ID = (uint8)Mat;
    if (ID >= GMatDataCount) return FText::GetEmpty();
    return FText::FromString(GMatNames[ID]);
}

EItemId FMaterialRegistry::GetFragmentItem(EMaterialType Mat)
{
    uint8 ID = (uint8)Mat;
    if (ID >= GMatDataCount) return EItemId::None;
    return GMatData[ID].FragmentItem;
}

float FMaterialRegistry::GetBrittleness(EMaterialType Mat)
{
    uint8 ID = (uint8)Mat;
    if (ID >= GMatDataCount) return 1.0f;
    return GMatData[ID].Brittleness;
}

// PHYS-1：EMaterialType → /Game/PhysicalMaterials/PM_{Name} 軟參照路徑
// 命名對應 generate_physical_materials.py 建立的資產名稱
// 液體/氣體材質保留路徑（PM 佔位），浮力仍由 APhysicalItemActor::Tick 處理
FSoftObjectPath FMaterialRegistry::GetPhysicalMaterialPath(EMaterialType Mat)
{
#define PHYS_MAT_PATH(Name) FSoftObjectPath(TEXT("/Game/PhysicalMaterials/" Name "." Name))
    switch (Mat)
    {
    case EMaterialType::Stone_Cobble:     return PHYS_MAT_PATH("PM_Stone_Cobble");
    case EMaterialType::Dirt_Dry:         return PHYS_MAT_PATH("PM_Dirt_Dry");
    case EMaterialType::Grass:            return PHYS_MAT_PATH("PM_Grass");
    case EMaterialType::Sand:             return PHYS_MAT_PATH("PM_Sand");
    case EMaterialType::Water:            return PHYS_MAT_PATH("PM_Water");
    case EMaterialType::Lava:             return PHYS_MAT_PATH("PM_Lava");
    case EMaterialType::Wood:             return PHYS_MAT_PATH("PM_Wood");
    case EMaterialType::Leaves:           return PHYS_MAT_PATH("PM_Leaves");
    case EMaterialType::Ore_Iron:         return PHYS_MAT_PATH("PM_Ore_Iron");
    case EMaterialType::Ore_Gold:         return PHYS_MAT_PATH("PM_Ore_Gold");
    case EMaterialType::Fire:             return PHYS_MAT_PATH("PM_Fire");
    case EMaterialType::Steam:            return PHYS_MAT_PATH("PM_Steam");
    case EMaterialType::Ash:              return PHYS_MAT_PATH("PM_Ash");
    case EMaterialType::Ore_Coal:         return PHYS_MAT_PATH("PM_Ore_Coal");
    case EMaterialType::Ore_Copper:       return PHYS_MAT_PATH("PM_Ore_Copper");
    case EMaterialType::Ore_MagicCrystal: return PHYS_MAT_PATH("PM_Ore_MagicCrystal");
    case EMaterialType::Fixture:          return PHYS_MAT_PATH("PM_Fixture");
    case EMaterialType::FallenLeaf:       return PHYS_MAT_PATH("PM_FallenLeaf");
    default:                              return PHYS_MAT_PATH("PM_Default");
    }
#undef PHYS_MAT_PATH
}

// ── 預設掉落查找（對應 Godot MaterialData.DefaultDrops）─────────────────
TArray<FItemDrop> FMaterialRegistry::GetDefaultDrops(EMaterialType Mat)
{
    TArray<FItemDrop> Out;
    switch (Mat)
    {
    // 2026-06-23 修復：單格採掘（DestroyReason::Mining）對應 Godot MaterialRegistry.cs:26-50
    // 的 DefaultDrops，掉的是完整方塊（BlockXxx×1），不是碎片——FragmentXxx 只該由
    // SpawnFragments()（材質單位批次換算）產生，本表先前誤填成 Fragment 系列。
    case EMaterialType::Stone_Cobble:
        Out.Add(FItemDrop(EItemId::BlockStone, 1, 1));
        break;
    case EMaterialType::Dirt_Dry:
        Out.Add(FItemDrop(EItemId::BlockDirt, 1, 1));
        break;
    case EMaterialType::Grass:
        Out.Add(FItemDrop(EItemId::BlockDirt, 1, 1));
        break;
    case EMaterialType::Sand:
        Out.Add(FItemDrop(EItemId::BlockSand, 1, 1));
        break;
    case EMaterialType::Wood:
        // 樹幹掉落橡木原木（W-A，原為 BlockWood）
        Out.Add(FItemDrop(EItemId::OakLog, 1, 1));
        break;
    case EMaterialType::Leaves:
        // 主掉落：落葉；機率掉落：橡木樹苗(5%)、橡木果實(10%)
        Out.Add(FItemDrop(EItemId::FallenLeaf,  1, 2, 1.0f));
        Out.Add(FItemDrop(EItemId::OakSapling,  1, 1, 0.05f));
        Out.Add(FItemDrop(EItemId::OakFruit,    1, 1, 0.10f));
        break;
    case EMaterialType::FallenLeaf:
        Out.Add(FItemDrop(EItemId::FallenLeaf, 1, 1));
        break;
    case EMaterialType::Ash:
        Out.Add(FItemDrop(EItemId::BlockAsh, 1, 1));
        break;
    // 礦石數量對齊 Godot MaterialRegistry.cs:56-69（Coal/Copper/Iron 皆 1~2，原 UE5 誤填 1~3）
    case EMaterialType::Ore_Iron:
        Out.Add(FItemDrop(EItemId::OreIronRaw, 1, 2));
        break;
    case EMaterialType::Ore_Coal:
        Out.Add(FItemDrop(EItemId::OreCoal, 1, 2));
        break;
    case EMaterialType::Ore_Copper:
        Out.Add(FItemDrop(EItemId::OreCopperRaw, 1, 2));
        break;
    case EMaterialType::Ore_Gold:
        // Godot 沒有金礦（UE5 新增），沿用其他礦石同款 1~2 區間，無 Godot 數值可對照
        Out.Add(FItemDrop(EItemId::OreGoldRaw, 1, 2));
        break;
    case EMaterialType::Ore_MagicCrystal:
        // 對齊 Godot MaterialRegistry.cs:74（1~1，原 UE5 誤填 1~2）
        Out.Add(FItemDrop(EItemId::OreMagicCrystal, 1, 1));
        break;
    default: break;
    }
    return Out;
}
