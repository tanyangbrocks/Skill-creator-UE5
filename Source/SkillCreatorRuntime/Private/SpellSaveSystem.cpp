#include "SpellSaveSystem.h"
#include "JsonObjectConverter.h"

FString FSpellSaveSystem::SaveGroupToString(const FSpellGroup& Group)
{
    FString Out;
    FJsonObjectConverter::UStructToJsonObjectString(Group, Out, 0, 0, 0, nullptr, true);
    return Out;
}

bool FSpellSaveSystem::LoadGroupFromString(const FString& Json, FSpellGroup& OutGroup)
{
    if (Json.IsEmpty()) return false;
    FSpellGroup Parsed;
    if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Parsed, 0, 0))
        return false;
    OutGroup = MoveTemp(Parsed);
    return true;
}
