#pragma once
#include "CoreMinimal.h"

// 世界存檔元資料（純文字 key=value 格式）
// 注意：chunk 二進制存檔由 FTileWorld3D::SaveChunk/TryLoadChunk 管理
struct VOXELWORLD_API FWorldSaveData
{
    FString    Id;
    FString    Name;
    int32      Seed          = 12345;
    FString    WorldDir;
    FIntVector PlayerSpawn   = FIntVector(0, 0, 0);
    bool       bIsFirstEnter = true;

    // chunks/{cx}_{cy}_{cz}.bin 路徑計算
    static FString GetChunkPath(const FString& InWorldDir, int32 cx, int32 cy, int32 cz);

    bool        SaveMeta(const FString& Path) const;
    static bool LoadMeta(const FString& Path, FWorldSaveData& Out);
};
