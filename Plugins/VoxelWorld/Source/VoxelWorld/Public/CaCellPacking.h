#pragma once
#include "CoreMinimal.h"
#include "MaterialRegistry.h"
#include "MaterialType.h"

// M-10：GPU CA buffer 的 32-bit cell 打包格式，對應 CaMargolus.usf 開頭的 bit 配置註解，
// 跟 Godot CaGpuSimulator.cs:40-49 完全一致：
//   bits  0- 7 : MaterialType
//   bits  8- 9 : physCompact (0=靜/空, 1=粉末, 2=液體)
//   bits 10-13 : density4 (0-15，從 MaterialData.Density×1.5 換算)
//   bit  14    : dirty flag
//   bits 16-23 : Timer
//   bits 24-30 : Variant
// 跟 FTileCell（5-byte CPU 格式，MaterialType.h）是兩套獨立但對應明確的表示法，
// 這裡只負責兩者之間的打包/解包轉換。對應 Godot CaGpuSimulator.cs:51-69,73-84 的
// _physBits[] 靜態查表 + Pack()/Unpack()，但這裡改用既有的 FMaterialRegistry 即時查，
// 不另外維護一份查表（材質不多，即時查的成本可忽略，且不用煩惱查表何時初始化）。
//
// 放在 VoxelWorld 模組（不是 VoxelWorldShaders）：這裡需要 FMaterialRegistry，
// VoxelWorldShaders 不能依賴 VoxelWorld（VoxelWorld 已經依賴 VoxelWorldShaders，
// 反過來會循環依賴），所以「打包/解包」跟「shader 類別本身」分屬不同模組——
// VoxelWorldShaders 只管「給一段 raw uint32 buffer，照 Margolus 規則搬動」，
// 完全不知道遊戲材質是什麼，這個職責切分本身是對的，不是妥協。
namespace CaCellPacking
{
    constexpr uint32 DirtyBit = 1u << 14;

    inline uint32 Pack(uint8 MaterialID, uint8 Timer = 0, uint8 Variant = 0)
    {
        const EPhysicsCategory Phys = FMaterialRegistry::GetPhysics(MaterialID);
        const uint32 PhysBits = (Phys == EPhysicsCategory::Powder) ? 1u
                               : (Phys == EPhysicsCategory::Liquid) ? 2u : 0u;
        const float DensityF = FMaterialRegistry::Get(static_cast<EMaterialType>(MaterialID)).Density;
        const uint32 DensBits = static_cast<uint32>(FMath::Clamp(static_cast<int32>(DensityF * 1.5f), 0, 15));

        return static_cast<uint32>(MaterialID)
             | (PhysBits << 8)
             | (DensBits << 10)
             | (static_cast<uint32>(Timer) << 16)
             | (static_cast<uint32>(Variant) << 24);
    }

    inline uint8 UnpackMaterialID(uint32 Packed) { return static_cast<uint8>(Packed & 0xFFu); }
    inline uint8 UnpackTimer(uint32 Packed)      { return static_cast<uint8>((Packed >> 16) & 0xFFu); }
    inline uint8 UnpackVariant(uint32 Packed)    { return static_cast<uint8>((Packed >> 24) & 0xFFu); }
    inline bool  IsDirty(uint32 Packed)          { return (Packed & DirtyBit) != 0; }
}
