#pragma once
#include "CoreMinimal.h"
#include "SpellLoadout.h"
#include "SpellGroup.generated.h"

// 對應 Godot SpellGroup.cs
// 5 組技能配置（V 鍵切換組）
USTRUCT(BlueprintType)
struct FSpellGroup
{
    GENERATED_BODY()

    static constexpr int32 MaxGroups = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SpellGroup")
    TArray<FSpellLoadout> Groups;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SpellGroup")
    int32 ActiveGroupIndex = 0;

    FSpellGroup() { Groups.SetNum(MaxGroups); }

    FSpellLoadout& GetActiveLoadout()
    {
        return Groups.IsValidIndex(ActiveGroupIndex) ? Groups[ActiveGroupIndex] : Groups[0];
    }

    const FSpellLoadout& GetActiveLoadout() const
    {
        return Groups.IsValidIndex(ActiveGroupIndex) ? Groups[ActiveGroupIndex] : Groups[0];
    }

    FSpellLoadout& GetGroup(int32 i)
    {
        return Groups.IsValidIndex(i) ? Groups[i] : Groups[0];
    }

    const FSpellLoadout& GetGroup(int32 i) const
    {
        return Groups.IsValidIndex(i) ? Groups[i] : Groups[0];
    }

    void SetActiveGroup(int32 i)
    {
        ActiveGroupIndex = FMath::Clamp(i, 0, MaxGroups - 1);
    }
};
