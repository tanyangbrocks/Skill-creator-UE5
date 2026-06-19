#include "SpellSaveSystem.h"
#include "FBlockNodeSaveData.h"
#include "JsonObjectConverter.h"
#include "ManaTypeRegistry.h"

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

// 清除未知 ManaTypeKey（對應 Godot SaveSystem.cs:282-287：還原時若 ManaTypeRegistry.Get()
// 找不到對應 key，清空並警告，避免已移除/拼錯的 MP 類型 key 無聲流入遊戲邏輯）
// 2026-06-20 Round3 D-11 修正：原本完全沒有此防禦邏輯。
static void ValidateManaTypeKeys(FSpellArray& S)
{
    for (FSpellSlot& Slot : S.Slots)
    {
        if (Slot.ManaTypeKey.IsNone()) continue;
        if (!FManaTypeRegistry::Get().Find(Slot.ManaTypeKey))
        {
            UE_LOG(LogTemp, Warning, TEXT("[SpellSaveSystem] 未知 ManaTypeKey '%s'，已清除"),
                   *Slot.ManaTypeKey.ToString());
            Slot.ManaTypeKey = NAME_None;
        }
    }
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
    ForEachSpellArray(Parsed, &ValidateManaTypeKeys);

    OutGroup = MoveTemp(Parsed);
    return true;
}
