#include "NPCIdentity.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"

FString FNPCIdentity::FilePath(FName InNPCId)
{
	return FPaths::ProjectSavedDir() / TEXT("NPCBrain") / TEXT("Identities")
		/ (InNPCId.ToString() + TEXT(".json"));
}

bool FNPCIdentity::Save() const
{
	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(*this, Json, 0, 0, 0, nullptr, true))
		return false;

	const FString Path = FilePath(NPCId);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return FFileHelper::SaveStringToFile(Json, *Path,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool FNPCIdentity::Load(FName InNPCId, FNPCIdentity& Out)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *FilePath(InNPCId))) return false;

	// Pre-set NPCId so it survives even though the JSON on disk won't contain it
	// under most code paths — JsonObjectStringToUStruct only overwrites fields
	// present in the JSON, matching FCharacterSaveData's Load() convention.
	Out.NPCId = InNPCId;
	return FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Out, 0, 0);
}
