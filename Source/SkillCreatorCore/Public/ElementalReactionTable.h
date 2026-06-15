#pragma once
#include "CoreMinimal.h"
#include "ElementType.h"
#include "ElementalStatusEffect.h"

// 單條反應定義
struct FReactionDef
{
    FString Name;
    // null = 純 CA 效果（W-3b 填入），不產生元素狀態效果
    TFunction<TUniquePtr<FElementalStatusEffect>()> MakeStatusEffect;
};

// 靜態查表（22 條反應，A+B 與 B+A 等價；對應 Godot ElementalReactionTable.cs）
// 不用 UObject — 純邏輯查表，無 gameplay 依賴。
class SKILLCREATORCORE_API FElementalReactionTable
{
public:
    // 回傳 null = 兩元素無反應定義
    static const FReactionDef* Lookup(ESkillElementType A, ESkillElementType B);
};
