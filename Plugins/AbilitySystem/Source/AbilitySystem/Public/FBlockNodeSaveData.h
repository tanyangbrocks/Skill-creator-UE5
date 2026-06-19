#pragma once
#include "CoreMinimal.h"
#include "BlockType.h"
#include "FBlockNode.h"
#include "FBlockNodeSaveData.generated.h"

// 一個積木節點的 JSON 可序列化鏡射版本。
// ⚠️ UHT 不支援 USTRUCT 透過 TArray 自我遞迴（"'Struct' recursion via arrays is unsupported"），
//    所以分支不能直接存 TArray<FBlockNodeSaveData>，改存 index，實際節點全部攤平在 FBlockTreeSaveData::Pool。
// Params 以 "StructPath|ExportedText" 字串形式儲存（FInstancedStruct 本身不可直接 UPROPERTY 序列化任意型別）。
USTRUCT()
struct ABILITYSYSTEM_API FBlockNodeSaveData
{
    GENERATED_BODY()

    UPROPERTY() uint8 TypeOrdinal = 0;
    UPROPERTY() TMap<FName, FString> ParamsSerialized;
    UPROPERTY() TArray<int32> ThenBranch;   // Pool 索引
    UPROPERTY() TArray<int32> ElseBranch;   // Pool 索引
    UPROPERTY() TArray<int32> LoopBody;     // Pool 索引
};

// 一整棵積木樹（多個根節點）攤平後的儲存格式。供 FSpellArray.BlocksJson 直接序列化。
USTRUCT()
struct ABILITYSYSTEM_API FBlockTreeSaveData
{
    GENERATED_BODY()

    UPROPERTY() TArray<FBlockNodeSaveData> Pool;
    UPROPERTY() TArray<int32>              Roots;

    static FBlockTreeSaveData             FromBlocks(const TArray<TUniquePtr<FBlockNode>>& Blocks);
    static TArray<TUniquePtr<FBlockNode>> ToBlocks(const FBlockTreeSaveData& Data);

private:
    // 將節點（含子樹）攤平加入 Pool，回傳此節點在 Pool 中的索引
    static int32 AppendNode(const FBlockNode& Node, TArray<FBlockNodeSaveData>& Pool);

    // 由 Pool 中指定索引還原出一個 FBlockNode（含子樹）
    static TUniquePtr<FBlockNode> BuildNode(int32 Index, const TArray<FBlockNodeSaveData>& Pool);
};
