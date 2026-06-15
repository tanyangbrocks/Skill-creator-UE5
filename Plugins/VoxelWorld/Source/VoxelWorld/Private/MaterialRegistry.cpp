#include "MaterialRegistry.h"

// 材質資料表（與 EMaterialType 一一對應，依 ID 順序）
static const FMaterialData GMatData[] =
{
    // ID  0 — Air
    { EPhysicsCategory::Empty,  0.f,  false, 0,   0   },
    // ID  1 — Stone
    { EPhysicsCategory::Static, 0.f,  false, 0,   0   },
    // ID  2 — Dirt
    { EPhysicsCategory::Static, 0.f,  false, 0,   0   },
    // ID  3 — Grass
    { EPhysicsCategory::Static, 0.f,  false, 0,   0   },
    // ID  4 — Sand
    { EPhysicsCategory::Powder, 0.f,  false, 0,   0   },
    // ID  5 — Water
    { EPhysicsCategory::Liquid, 1.0f, false, 0,   0   },
    // ID  6 — Lava
    { EPhysicsCategory::Liquid, 3.0f, false, 0,   0   },
    // ID  7 — Wood
    { EPhysicsCategory::Static, 0.f,  true,  80,  200 },
    // ID  8 — Leaves
    { EPhysicsCategory::Static, 0.f,  true,  20,  60  },
    // ID  9 — Ore_Iron
    { EPhysicsCategory::Static, 0.f,  false, 0,   0   },
    // ID 10 — Ore_Gold
    { EPhysicsCategory::Static, 0.f,  false, 0,   0   },
    // ID 11 — Fire
    { EPhysicsCategory::Gas,    0.f,  false, 0,   0   },
    // ID 12 — Steam
    { EPhysicsCategory::Gas,    0.f,  false, 0,   0   },
    // ID 13 — Ash
    { EPhysicsCategory::Static, 0.f,  false, 0,   0   },
    // ID 14 — Ore_Coal
    { EPhysicsCategory::Static, 0.f,  true,  180, 240 },
    // ID 15 — Ore_Copper
    { EPhysicsCategory::Static, 0.f,  false, 0,   0   },
    // ID 16 — Ore_MagicCrystal
    { EPhysicsCategory::Static, 0.f,  false, 0,   0   },
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
