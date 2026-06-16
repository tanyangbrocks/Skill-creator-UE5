#include "MaterialRegistry.h"

using E = ESkillElementType;
using P = EPhysicsCategory;

using I = EItemId;

// 材質資料表（與 EMaterialType 一一對應，依 ID 順序）
// 欄位順序：Physics, Density, bFlammable, BurnMin, BurnMax, NativeElement,
//           bIsMineable, Hardness, RequiredToolTier, BlastResistance, MagicResistance, Opacity,
//           FragmentItem
static const FMaterialData GMatData[] =
{
    // ID  0 — Air
    { P::Empty,  0.f,  false, 0,   0,   E::None,  false, 0.f, 0, 0.f,  0.f,  0,   I::None           },
    // ID  1 — Stone
    { P::Static, 0.f,  false, 0,   0,   E::Earth, true,  3.f, 1, 2.f,  0.5f, 255, I::FragmentStone  },
    // ID  2 — Dirt
    { P::Static, 0.f,  false, 0,   0,   E::Earth, true,  1.f, 0, 0.5f, 0.f,  255, I::FragmentDirt   },
    // ID  3 — Grass
    { P::Static, 0.f,  false, 0,   0,   E::Wood,  true,  1.f, 0, 0.5f, 0.f,  255, I::FragmentDirt   },
    // ID  4 — Sand
    { P::Powder, 0.f,  false, 0,   0,   E::Earth, true,  0.5f,0, 0.2f, 0.f,  255, I::FragmentSand   },
    // ID  5 — Water
    { P::Liquid, 1.0f, false, 0,   0,   E::Water, false, 0.f, 0, 0.f,  0.2f, 200, I::None           },
    // ID  6 — Lava
    { P::Liquid, 3.0f, false, 0,   0,   E::Fire,  false, 0.f, 0, 0.f,  0.5f, 220, I::None           },
    // ID  7 — Wood
    { P::Static, 0.f,  true,  80,  200, E::Wood,  true,  1.5f,1, 0.5f, 0.f,  255, I::FragmentWood   },
    // ID  8 — Leaves
    { P::Static, 0.f,  true,  20,  60,  E::Wood,  true,  0.5f,0, 0.2f, 0.f,  180, I::FragmentWood   },
    // ID  9 — Ore_Iron
    { P::Static, 0.f,  false, 0,   0,   E::Metal, true,  4.f, 2, 3.f,  1.f,  255, I::OreIronRaw     },
    // ID 10 — Ore_Gold
    { P::Static, 0.f,  false, 0,   0,   E::Metal, true,  3.5f,2, 2.5f, 0.5f, 255, I::None           },
    // ID 11 — Fire
    { P::Gas,    0.f,  false, 0,   0,   E::Fire,  false, 0.f, 0, 0.f,  0.5f, 150, I::None           },
    // ID 12 — Steam
    { P::Gas,    0.f,  false, 0,   0,   E::Water, false, 0.f, 0, 0.f,  0.f,  100, I::None           },
    // ID 13 — Ash
    { P::Static, 0.f,  false, 0,   0,   E::None,  true,  0.3f,0, 0.1f, 0.f,  255, I::FragmentAsh    },
    // ID 14 — Ore_Coal
    { P::Static, 0.f,  true,  180, 240, E::Earth, true,  3.f, 1, 2.f,  0.5f, 255, I::OreCoal        },
    // ID 15 — Ore_Copper
    { P::Static, 0.f,  false, 0,   0,   E::Metal, true,  3.5f,1, 2.5f, 0.5f, 255, I::OreCopperRaw   },
    // ID 16 — Ore_MagicCrystal
    { P::Static, 0.f,  false, 0,   0,   E::Light, true,  4.5f,3, 2.f,  3.f,  210, I::OreMagicCrystal},
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
};

FLinearColor FMaterialRegistry::GetColor(EMaterialType Mat)
{
    uint8 ID = (uint8)Mat;
    if (ID >= GMatDataCount) return FLinearColor::White;
    return GMatColors[ID];
}

// ── 顯示名稱查找表（中文）────────────────────────────────────────────────
static const TCHAR* const GMatNames[] =
{
    TEXT("空氣"),     TEXT("石頭"),   TEXT("泥土"),   TEXT("草地"),   TEXT("沙"),
    TEXT("水"),       TEXT("熔岩"),   TEXT("木頭"),   TEXT("樹葉"),   TEXT("鐵礦石"),
    TEXT("金礦石"),   TEXT("火焰"),   TEXT("蒸汽"),   TEXT("灰燼"),   TEXT("煤礦"),
    TEXT("銅礦石"),   TEXT("魔法水晶"),
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

// ── 預設掉落查找（對應 Godot MaterialData.DefaultDrops）─────────────────
TArray<FItemDrop> FMaterialRegistry::GetDefaultDrops(EMaterialType Mat)
{
    TArray<FItemDrop> Out;
    switch (Mat)
    {
    case EMaterialType::Stone:
        Out.Add(FItemDrop(EItemId::FragmentStone, 1, 2));
        break;
    case EMaterialType::Dirt:
        Out.Add(FItemDrop(EItemId::FragmentDirt, 1, 1));
        break;
    case EMaterialType::Sand:
        Out.Add(FItemDrop(EItemId::FragmentSand, 1, 1));
        break;
    case EMaterialType::Wood:
        Out.Add(FItemDrop(EItemId::FragmentWood, 1, 2));
        break;
    case EMaterialType::Ash:
        Out.Add(FItemDrop(EItemId::FragmentAsh, 1, 1));
        break;
    case EMaterialType::Ore_Iron:
        Out.Add(FItemDrop(EItemId::OreIronRaw, 1, 3));
        break;
    case EMaterialType::Ore_Coal:
        Out.Add(FItemDrop(EItemId::OreCoal, 1, 3));
        break;
    case EMaterialType::Ore_Copper:
        Out.Add(FItemDrop(EItemId::OreCopperRaw, 1, 3));
        break;
    case EMaterialType::Ore_MagicCrystal:
        Out.Add(FItemDrop(EItemId::OreMagicCrystal, 1, 2));
        break;
    default: break;
    }
    return Out;
}
