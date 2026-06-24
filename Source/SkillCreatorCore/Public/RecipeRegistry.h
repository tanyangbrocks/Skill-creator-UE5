#pragma once
#include "CoreMinimal.h"
#include "ItemId.h"
#include "RecipeRegistry.generated.h"

// 加工配方（docs/plan-item-crafting-system.md §八）
USTRUCT(BlueprintType)
struct FRecipeInput
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EItemId ItemId = EItemId::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32   Count  = 1;
};

USTRUCT(BlueprintType)
struct FRecipeEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EItemId Output      = EItemId::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32   OutputCount = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FRecipeInput> Inputs;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool    bRequiresWorkbench = false;
};

// 對應 Godot 模式：靜態登錄表，仿 FItemRegistry::Init（function-local static，首次呼叫初始化）
struct SKILLCREATORCORE_API FRecipeRegistry
{
    static const TArray<FRecipeEntry>& GetAll();
private:
    static void Init(TArray<FRecipeEntry>& Out);
};
