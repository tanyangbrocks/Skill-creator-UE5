#include "MaterialRegistry.h"

using E = ESkillElementType;
using P = EPhysicsCategory;

// 材質資料表（與 EMaterialType 一一對應，依 ID 順序）
// 欄位順序：Physics, Density, bFlammable, BurnMin, BurnMax, NativeElement,
//           bIsMineable, Hardness, RequiredToolTier, BlastResistance, MagicResistance, Opacity
static const FMaterialData GMatData[] =
{
    // ID  0 — Air
    { P::Empty,  0.f,  false, 0,   0,   E::None,  false, 0.f, 0, 0.f,  0.f,  0   },
    // ID  1 — Stone
    { P::Static, 0.f,  false, 0,   0,   E::Earth, true,  3.f, 1, 2.f,  0.5f, 255 },
    // ID  2 — Dirt
    { P::Static, 0.f,  false, 0,   0,   E::Earth, true,  1.f, 0, 0.5f, 0.f,  255 },
    // ID  3 — Grass
    { P::Static, 0.f,  false, 0,   0,   E::Wood,  true,  1.f, 0, 0.5f, 0.f,  255 },
    // ID  4 — Sand
    { P::Powder, 0.f,  false, 0,   0,   E::Earth, true,  0.5f,0, 0.2f, 0.f,  255 },
    // ID  5 — Water
    { P::Liquid, 1.0f, false, 0,   0,   E::Water, false, 0.f, 0, 0.f,  0.2f, 200 },
    // ID  6 — Lava
    { P::Liquid, 3.0f, false, 0,   0,   E::Fire,  false, 0.f, 0, 0.f,  0.5f, 220 },
    // ID  7 — Wood
    { P::Static, 0.f,  true,  80,  200, E::Wood,  true,  1.5f,1, 0.5f, 0.f,  255 },
    // ID  8 — Leaves
    { P::Static, 0.f,  true,  20,  60,  E::Wood,  true,  0.5f,0, 0.2f, 0.f,  180 },
    // ID  9 — Ore_Iron
    { P::Static, 0.f,  false, 0,   0,   E::Metal, true,  4.f, 2, 3.f,  1.f,  255 },
    // ID 10 — Ore_Gold
    { P::Static, 0.f,  false, 0,   0,   E::Metal, true,  3.5f,2, 2.5f, 0.5f, 255 },
    // ID 11 — Fire
    { P::Gas,    0.f,  false, 0,   0,   E::Fire,  false, 0.f, 0, 0.f,  0.5f, 150 },
    // ID 12 — Steam
    { P::Gas,    0.f,  false, 0,   0,   E::Water, false, 0.f, 0, 0.f,  0.f,  100 },
    // ID 13 — Ash
    { P::Static, 0.f,  false, 0,   0,   E::None,  true,  0.3f,0, 0.1f, 0.f,  255 },
    // ID 14 — Ore_Coal
    { P::Static, 0.f,  true,  180, 240, E::Earth, true,  3.f, 1, 2.f,  0.5f, 255 },
    // ID 15 — Ore_Copper
    { P::Static, 0.f,  false, 0,   0,   E::Metal, true,  3.5f,1, 2.5f, 0.5f, 255 },
    // ID 16 — Ore_MagicCrystal
    { P::Static, 0.f,  false, 0,   0,   E::Light, true,  4.5f,3, 2.f,  3.f,  210 },
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
