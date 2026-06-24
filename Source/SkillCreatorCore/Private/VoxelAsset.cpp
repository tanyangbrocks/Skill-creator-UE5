#include "VoxelAsset.h"

EMaterialType UVoxelMaterialPalette::Resolve(uint8 PaletteIndex) const
{
    if (PaletteIndex == 0 || PaletteIndex >= static_cast<uint8>(PaletteToMaterial.Num()))
        return EMaterialType::Air;
    return PaletteToMaterial[PaletteIndex];
}
