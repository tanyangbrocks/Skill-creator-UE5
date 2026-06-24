#include "VoxParser.h"

namespace VoxParser
{

static constexpr uint32 VOX_MAGIC   = 0x20584F56; // "VOX "
static constexpr uint32 VOX_VERSION = 150;

// 調色盤公約：index = EMaterialType 的底層值（0=Air 保留）
static EMaterialType DefaultPaletteResolve(uint8 PaletteIndex)
{
    if (PaletteIndex == 0 || PaletteIndex >= static_cast<uint8>(EMaterialType::Count))
        return EMaterialType::Air;
    return static_cast<EMaterialType>(PaletteIndex);
}

static bool ReadChunkId(const TArray<uint8>& Raw, int32& Pos, char OutId[5])
{
    if (Pos + 4 > Raw.Num()) return false;
    FMemory::Memcpy(OutId, Raw.GetData() + Pos, 4);
    OutId[4] = 0;
    Pos += 4;
    return true;
}

static bool ReadInt32(const TArray<uint8>& Raw, int32& Pos, int32& Out)
{
    if (Pos + 4 > Raw.Num()) return false;
    FMemory::Memcpy(&Out, Raw.GetData() + Pos, 4);
    Pos += 4;
    return true;
}

bool Parse(const TArray<uint8>& Raw,
           TArray<FVoxelCell>& OutCells,
           FIntVector& OutBoundsMax)
{
    OutCells.Reset();
    OutBoundsMax = FIntVector::ZeroValue;

    if (Raw.Num() < 12) return false;

    int32 Pos = 0;

    uint32 Magic = 0, Version = 0;
    FMemory::Memcpy(&Magic,   Raw.GetData() + 0, 4);
    FMemory::Memcpy(&Version, Raw.GetData() + 4, 4);
    Pos = 8;
    if (Magic != VOX_MAGIC || Version != VOX_VERSION) return false;

    char Id[5];
    int32 ContentSize = 0, ChildrenSize = 0;
    if (!ReadChunkId(Raw, Pos, Id)) return false;
    if (!ReadInt32(Raw, Pos, ContentSize))  return false;
    if (!ReadInt32(Raw, Pos, ChildrenSize)) return false;
    Pos += ContentSize;

    int32 SizeX = 0, SizeY = 0, SizeZ = 0;
    bool bHasSize = false;
    TArray<uint8> VoxelBuffer;

    const int32 End = FMath::Min(Pos + ChildrenSize, Raw.Num());
    while (Pos < End)
    {
        if (!ReadChunkId(Raw, Pos, Id)) break;
        int32 CSize = 0, CChildren = 0;
        if (!ReadInt32(Raw, Pos, CSize))     break;
        if (!ReadInt32(Raw, Pos, CChildren)) break;

        if (FMemory::Memcmp(Id, "SIZE", 4) == 0)
        {
            ReadInt32(Raw, Pos, SizeX);
            ReadInt32(Raw, Pos, SizeY);
            ReadInt32(Raw, Pos, SizeZ);
            bHasSize = true;
        }
        else if (FMemory::Memcmp(Id, "XYZI", 4) == 0)
        {
            int32 Num = 0;
            if (!ReadInt32(Raw, Pos, Num)) break;
            const int32 BytesNeeded = Num * 4;
            if (Pos + BytesNeeded > Raw.Num()) break;
            VoxelBuffer.SetNumUninitialized(BytesNeeded);
            FMemory::Memcpy(VoxelBuffer.GetData(), Raw.GetData() + Pos, BytesNeeded);
            Pos += BytesNeeded;
        }
        else
        {
            Pos += CSize;
        }
        Pos += CChildren;
    }

    if (!bHasSize || VoxelBuffer.Num() == 0) return false;

    OutBoundsMax = FIntVector(SizeX, SizeZ, SizeY);

    const int32 VoxCount = VoxelBuffer.Num() / 4;
    OutCells.Reserve(VoxCount);
    for (int32 i = 0; i < VoxCount; ++i)
    {
        const uint8 mx = VoxelBuffer[i * 4 + 0];
        const uint8 my = VoxelBuffer[i * 4 + 1];
        const uint8 mz = VoxelBuffer[i * 4 + 2];
        const uint8 pi = VoxelBuffer[i * 4 + 3];

        EMaterialType Mat = DefaultPaletteResolve(pi);
        if (Mat == EMaterialType::Air) continue;

        FVoxelCell Cell;
        Cell.X        = static_cast<int16>(mx);
        Cell.Y        = static_cast<int16>(SizeZ - 1 - mz);
        Cell.Z        = static_cast<int16>(my);
        Cell.Material = Mat;
        OutCells.Add(Cell);
    }

    return true;
}

} // namespace VoxParser
