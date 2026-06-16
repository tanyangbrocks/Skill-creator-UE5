#pragma once
#include "CoreMinimal.h"
#include "ItemId.h"
#include "ItemStack.generated.h"

// 物品堆疊（對應 Godot ItemStack.cs）
USTRUCT(BlueprintType)
struct SKILLCREATORCORE_API FItemStack
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EItemId ItemId = EItemId::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32   Count  = 0;

    bool IsEmpty() const { return ItemId == EItemId::None || Count <= 0; }

    FItemStack() = default;
    FItemStack(EItemId Id, int32 C) : ItemId(Id), Count(C) {}

    FItemStack WithCount(int32 NewCount) const { return FItemStack(ItemId, NewCount); }

    static FItemStack Empty() { return FItemStack(); }
};
