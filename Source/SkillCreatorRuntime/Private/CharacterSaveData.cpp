#include "CharacterSaveData.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "JsonObjectConverter.h"

FString FCharacterSaveData::FilePath(const FString& WorldDir)
{
    return WorldDir / TEXT("character.json");
}

bool FCharacterSaveData::Save(const FString& WorldDir) const
{
    FString Json;
    if (!FJsonObjectConverter::UStructToJsonObjectString(*this, Json, 0, 0, 0, nullptr, true))
        return false;

    IFileManager::Get().MakeDirectory(*WorldDir, true);
    return FFileHelper::SaveStringToFile(Json, *FilePath(WorldDir),
                                         FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool FCharacterSaveData::Load(const FString& WorldDir, FCharacterSaveData& Out)
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *FilePath(WorldDir))) return false;
    return FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Out, 0, 0);
}
