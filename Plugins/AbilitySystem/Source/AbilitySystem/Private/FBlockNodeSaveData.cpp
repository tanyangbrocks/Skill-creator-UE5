#include "FBlockNodeSaveData.h"
#include "UObject/Class.h"
#include "UObject/PropertyPortFlags.h"

int32 FBlockTreeSaveData::AppendNode(const FBlockNode& Node, TArray<FBlockNodeSaveData>& Pool)
{
    const int32 MyIndex = Pool.AddDefaulted();

    // 先攤平子樹（Pool 可能因 Add 而重新配置記憶體，故全程只用 index，不持有跨呼叫的參考/指標）
    TArray<int32> ThenIdx, ElseIdx, LoopIdx;
    for (const TUniquePtr<FBlockNode>& C : Node.ThenBranch) if (C) ThenIdx.Add(AppendNode(*C, Pool));
    for (const TUniquePtr<FBlockNode>& C : Node.ElseBranch) if (C) ElseIdx.Add(AppendNode(*C, Pool));
    for (const TUniquePtr<FBlockNode>& C : Node.LoopBody)   if (C) LoopIdx.Add(AppendNode(*C, Pool));

    FBlockNodeSaveData D;
    D.TypeOrdinal = (uint8)Node.Type;
    for (const TPair<FName, FInstancedStruct>& KV : Node.Params)
    {
        if (!KV.Value.IsValid()) continue;
        const UScriptStruct* SS = KV.Value.GetScriptStruct();
        FString ExportedText;
        SS->ExportText(ExportedText, KV.Value.GetMemory(), nullptr, nullptr, PPF_None, nullptr);
        D.ParamsSerialized.Add(KV.Key, SS->GetPathName() + TEXT("|") + ExportedText);
    }
    D.ThenBranch = MoveTemp(ThenIdx);
    D.ElseBranch = MoveTemp(ElseIdx);
    D.LoopBody   = MoveTemp(LoopIdx);

    Pool[MyIndex] = MoveTemp(D);
    return MyIndex;
}

TUniquePtr<FBlockNode> FBlockTreeSaveData::BuildNode(int32 Index, const TArray<FBlockNodeSaveData>& Pool)
{
    if (!Pool.IsValidIndex(Index)) return nullptr;
    const FBlockNodeSaveData& D = Pool[Index];

    TUniquePtr<FBlockNode> Node = MakeUnique<FBlockNode>();
    Node->Type = (EBlockType)D.TypeOrdinal;

    for (const TPair<FName, FString>& KV : D.ParamsSerialized)
    {
        int32 Sep;
        if (!KV.Value.FindChar(TEXT('|'), Sep)) continue;
        const FString TypePath = KV.Value.Left(Sep);
        const FString DataText = KV.Value.Mid(Sep + 1);

        UScriptStruct* SS = FindObject<UScriptStruct>(nullptr, *TypePath, EFindObjectFlags::ExactClass);
        if (!SS) continue;

        FInstancedStruct IS;
        IS.InitializeAs(SS);
        const TCHAR* Ptr = *DataText;
        SS->ImportText(Ptr, IS.GetMutableMemory(), nullptr, PPF_None, GLog, SS->GetName());
        Node->Params.Add(KV.Key, MoveTemp(IS));
    }

    for (int32 Idx : D.ThenBranch)
        if (TUniquePtr<FBlockNode> C = BuildNode(Idx, Pool)) Node->ThenBranch.Add(MoveTemp(C));
    for (int32 Idx : D.ElseBranch)
        if (TUniquePtr<FBlockNode> C = BuildNode(Idx, Pool)) Node->ElseBranch.Add(MoveTemp(C));
    for (int32 Idx : D.LoopBody)
        if (TUniquePtr<FBlockNode> C = BuildNode(Idx, Pool)) Node->LoopBody.Add(MoveTemp(C));

    return Node;
}

FBlockTreeSaveData FBlockTreeSaveData::FromBlocks(const TArray<TUniquePtr<FBlockNode>>& Blocks)
{
    FBlockTreeSaveData Tree;
    for (const TUniquePtr<FBlockNode>& B : Blocks)
        if (B) Tree.Roots.Add(AppendNode(*B, Tree.Pool));
    return Tree;
}

TArray<TUniquePtr<FBlockNode>> FBlockTreeSaveData::ToBlocks(const FBlockTreeSaveData& Data)
{
    TArray<TUniquePtr<FBlockNode>> Result;
    for (int32 RootIdx : Data.Roots)
        if (TUniquePtr<FBlockNode> Node = BuildNode(RootIdx, Data.Pool))
            Result.Add(MoveTemp(Node));
    return Result;
}
