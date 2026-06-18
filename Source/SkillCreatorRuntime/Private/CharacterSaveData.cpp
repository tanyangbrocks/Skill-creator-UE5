#include "CharacterSaveData.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "JsonObjectConverter.h"

FString FCharacterSaveData::FilePath(const FString& InId)
{
    return FPaths::ProjectSavedDir() / TEXT("Characters") / (InId + TEXT(".json"));
}

bool FCharacterSaveData::Save() const
{
    FString Json;
    if (!FJsonObjectConverter::UStructToJsonObjectString(*this, Json, 0, 0, 0, nullptr, true))
        return false;

    const FString Path = FilePath(Id);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    return FFileHelper::SaveStringToFile(Json, *Path,
                                         FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool FCharacterSaveData::Load(const FString& InId, FCharacterSaveData& Out)
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *FilePath(InId))) return false;
    Out.Id = InId;
    return FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Out, 0, 0);
}
