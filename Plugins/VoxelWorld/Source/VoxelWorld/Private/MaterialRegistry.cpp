#include "MaterialRegistry.h"

using E = ESkillElementType;

// 材質資料表（與 EMaterialType 一一對應，依 ID 順序）
// 欄位順序：Physics, Density, bFlammable, BurnMin, BurnMax, NativeElement
static const FMaterialData GMatData[] =
{
    // ID  0 — Air
    { EPhysicsCategory::Empty,  0.f,  false, 0,   0,   E::None  },
    // ID  1 — Stone
    { EPhysicsCategory::Static, 0.f,  false, 0,   0,   E::Earth },
    // ID  2 — Dirt
    { EPhysicsCategory::Static, 0.f,  false, 0,   0,   E::Earth },
    // ID  3 — Grass（UE5 新增，對應木屬性）
    { EPhysicsCategory::Static, 0.f,  false, 0,   0,   E::Wood  },
    // ID  4 — Sand
    { EPhysicsCategory::Powder, 0.f,  false, 0,   0,   E::Earth },
    // ID  5 — Water
    { EPhysicsCategory::Liquid, 1.0f, false, 0,   0,   E::Water },
    // ID  6 — Lava
    { EPhysicsCategory::Liquid, 3.0f, false, 0,   0,   E::Fire  },
    // ID  7 — Wood
    { EPhysicsCategory::Static, 0.f,  true,  80,  200, E::Wood  },
    // ID  8 — Leaves（UE5 新增，對應木屬性）
    { EPhysicsCategory::Static, 0.f,  true,  20,  60,  E::Wood  },
    // ID  9 — Ore_Iron
    { EPhysicsCategory::Static, 0.f,  false, 0,   0,   E::Metal },
    // ID 10 — Ore_Gold（UE5 新增，對應金屬屬性）
    { EPhysicsCategory::Static, 0.f,  false, 0,   0,   E::Metal },
    // ID 11 — Fire
    { EPhysicsCategory::Gas,    0.f,  false, 0,   0,   E::Fire  },
    // ID 12 — Steam（水蒸發產物，仍歸水屬性）
    { EPhysicsCategory::Gas,    0.f,  false, 0,   0,   E::Water },
    // ID 13 — Ash
    { EPhysicsCategory::Static, 0.f,  false, 0,   0,   E::None  },
    // ID 14 — Ore_Coal
    { EPhysicsCategory::Static, 0.f,  true,  180, 240, E::Earth },
    // ID 15 — Ore_Copper
    { EPhysicsCategory::Static, 0.f,  false, 0,   0,   E::Metal },
    // ID 16 — Ore_MagicCrystal
    { EPhysicsCategory::Static, 0.f,  false, 0,   0,   E::Light },
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
