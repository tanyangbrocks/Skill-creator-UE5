#include "SpellSaveSystem.h"
#include "FBlockNodeSaveData.h"
#include "JsonObjectConverter.h"

// 將一個 FSpellArray 的 Blocks（執行期 AST）寫入 BlocksJson（UPROPERTY，FJsonObjectConverter 可序列化）
static void PopulateBlocksJson(FSpellArray& S)
{
    if (!S.Blocks || S.Blocks->IsEmpty())
    {
        S.BlocksJson.Empty();
        return;
    }

    const FBlockTreeSaveData Tree = FBlockTreeSaveData::FromBlocks(*S.Blocks);
    FJsonObjectConverter::UStructToJsonObjectString(Tree, S.BlocksJson, 0, 0, 0, nullptr, true);
}

// 將 BlocksJson 還原為 Blocks（執行期 AST），供 SpellCompiler 編譯
static void RestoreBlocksFromJson(FSpellArray& S)
{
    if (S.BlocksJson.IsEmpty()) return;

    FBlockTreeSaveData Tree;
    if (!FJsonObjectConverter::JsonObjectStringToUStruct(S.BlocksJson, &Tree, 0, 0)) return;

    TArray<TUniquePtr<FBlockNode>> Blocks = FBlockTreeSaveData::ToBlocks(Tree);
    if (!Blocks.IsEmpty())
        S.SetBlocks(MoveTemp(Blocks));
}

static void ForEachSpellArray(FSpellGroup& Group, TFunctionRef<void(FSpellArray&)> Fn)
{
    for (FSpellLoadout& L : Group.Groups)
    {
        for (FSpellArray& S : L.Slots)        Fn(S);
        for (FSpellArray& S : L.PassiveSpells) Fn(S);
    }
}

FString FSpellSaveSystem::SaveGroupToString(const FSpellGroup& Group)
{
    FSpellGroup Mutable = Group; // 複製後填入 BlocksJson，不動原資料
    ForEachSpellArray(Mutable, &PopulateBlocksJson);

    FString Out;
    FJsonObjectConverter::UStructToJsonObjectString(Mutable, Out, 0, 0, 0, nullptr, true);
    return Out;
}

bool FSpellSaveSystem::LoadGroupFromString(const FString& Json, FSpellGroup& OutGroup)
{
    if (Json.IsEmpty()) return false;

    FSpellGroup Parsed;
    if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Parsed, 0, 0))
        return false;

    ForEachSpellArray(Parsed, &RestoreBlocksFromJson);

    OutGroup = MoveTemp(Parsed);
    return true;
}
