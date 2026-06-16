#pragma once
#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "BlockType.h"
#include "FBlockNode.generated.h"

// 積木 AST 節點：執行期型別（對應 Godot BlockNode.cs）
//
// ⚠️ 這是 runtime 執行用的型別，不是 M-9 Slate 編輯器用的節點型別。
//    編輯器型別是 UMyEdGraphNode : UEdGraphNode，載入/儲存時轉換。
//
// ⚠️ TArray<TUniquePtr<FBlockNode>> 分支欄位禁止加 UPROPERTY，
//    TUniquePtr 不支援 UHT 反射。Params 欄位可以加 UPROPERTY。
USTRUCT()
struct FBlockNode
{
    GENERATED_BODY()

    // 積木型別
    UPROPERTY() EBlockType Type = EBlockType::If;

    // 積木參數（設計時參數）
    // key = 參數名稱（FName 比 FString 快：lock-free FNamePool，無堆積分配）
    // value = 型別安全的任意結構（由 EBlockType 隱式決定預期型別）
    UPROPERTY() TMap<FName, FInstancedStruct> Params;

    // 分支子節點（TUniquePtr 防止意外複製整棵樹）
    // 不加 UPROPERTY — TUniquePtr 不支援 UHT 反射
    TArray<TUniquePtr<FBlockNode>> ThenBranch;   // If / Evaluate 的 then
    TArray<TUniquePtr<FBlockNode>> ElseBranch;   // If 的 else
    TArray<TUniquePtr<FBlockNode>> LoopBody;     // RepeatN / RepeatWhile

    // 移動語義（TUniquePtr 不可複製）
    FBlockNode() = default;
    FBlockNode(FBlockNode&&) = default;
    FBlockNode& operator=(FBlockNode&&) = default;
    FBlockNode(const FBlockNode&) = delete;
    FBlockNode& operator=(const FBlockNode&) = delete;

    // ── 靜態輔助：積木列表操作（對應 Godot BlockScript.SplitAt / AppendBlocks）──

    // 將 List 從 Index 處拆成兩段；[Index, end) 移入 RightOut，List 保留 [0, Index)
    static void SplitAt(TArray<TUniquePtr<FBlockNode>>& List, int32 Index,
                        TArray<TUniquePtr<FBlockNode>>& RightOut)
    {
        RightOut.Reset();
        for (int32 i = Index; i < List.Num(); ++i)
            RightOut.Add(MoveTemp(List[i]));
        List.SetNum(Index, EAllowShrinking::No);
    }

    // 將 Source 的全部節點移入 Target 末尾（Source 清空）
    static void AppendBlocks(TArray<TUniquePtr<FBlockNode>>& Target,
                             TArray<TUniquePtr<FBlockNode>>& Source)
    {
        for (TUniquePtr<FBlockNode>& Node : Source)
            Target.Add(MoveTemp(Node));
        Source.Reset();
    }
};

// UE5 USTRUCT 含不可複製成員的標準做法：
// 告知 TCppStructOps<FBlockNode> 不要嘗試產生複製操作
// 否則 UHT 自動生成的 FBlockNode.gen.cpp 會呼叫已 delete 的 operator=
template<>
struct TStructOpsTypeTraits<FBlockNode> : public TStructOpsTypeTraitsBase2<FBlockNode>
{
    enum { WithCopy = false };
};
