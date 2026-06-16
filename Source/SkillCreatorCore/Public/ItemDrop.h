#pragma once
#include "CoreMinimal.h"
#include "ItemId.h"
#include "ItemDrop.generated.h"

// 掉落定義（對應 Godot ItemDrop.cs）
USTRUCT(BlueprintType)
struct SKILLCREATORCORE_API FItemDrop
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EItemId ItemId   = EItemId::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32   MinCount = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32   MaxCount = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float   Chance   = 1.f;

    FItemDrop() = default;
    FItemDrop(EItemId Id, int32 Min, int32 Max, float C = 1.f)
        : ItemId(Id), MinCount(Min), MaxCount(Max), Chance(C) {}
};
