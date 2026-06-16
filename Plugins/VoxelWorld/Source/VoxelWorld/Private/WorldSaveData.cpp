#include "WorldSaveData.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

FString FWorldSaveData::GetChunkPath(const FString& InWorldDir, int32 cx, int32 cy, int32 cz)
{
    return FString::Printf(TEXT("%s/chunks/%d_%d_%d.bin"), *InWorldDir, cx, cy, cz);
}

bool FWorldSaveData::SaveMeta(const FString& Path) const
{
    FString Content = FString::Printf(
        TEXT("Id=%s\nName=%s\nSeed=%d\nWorldDir=%s\n"
             "SpawnX=%d\nSpawnY=%d\nSpawnZ=%d\nFirstEnter=%d\nLastPlayed=%s\n"),
        *Id, *Name, Seed, *WorldDir,
        PlayerSpawn.X, PlayerSpawn.Y, PlayerSpawn.Z,
        bIsFirstEnter ? 1 : 0,
        *LastPlayed.ToIso8601());
    return FFileHelper::SaveStringToFile(Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool FWorldSaveData::LoadMeta(const FString& Path, FWorldSaveData& Out)
{
    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *Path)) return false;

    TArray<FString> Lines;
    Content.ParseIntoArrayLines(Lines);
    for (const FString& Line : Lines)
    {
        FString Key, Val;
        if (!Line.Split(TEXT("="), &Key, &Val)) continue;
        if      (Key == TEXT("Id"))          Out.Id            = Val;
        else if (Key == TEXT("Name"))        Out.Name          = Val;
        else if (Key == TEXT("Seed"))        Out.Seed          = FCString::Atoi(*Val);
        else if (Key == TEXT("WorldDir"))    Out.WorldDir      = Val;
        else if (Key == TEXT("SpawnX"))      Out.PlayerSpawn.X = FCString::Atoi(*Val);
        else if (Key == TEXT("SpawnY"))      Out.PlayerSpawn.Y = FCString::Atoi(*Val);
        else if (Key == TEXT("SpawnZ"))      Out.PlayerSpawn.Z = FCString::Atoi(*Val);
        else if (Key == TEXT("FirstEnter"))  Out.bIsFirstEnter = FCString::Atoi(*Val) != 0;
        else if (Key == TEXT("LastPlayed"))  FDateTime::ParseIso8601(*Val, Out.LastPlayed);
    }
    return true;
}
