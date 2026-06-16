#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemStack.h"
#include "UInventoryComponent.generated.h"

// 背包組件（對應 Godot Inventory.cs）
// 30 格：前 10 格為熱鍵欄，後 20 格為主欄。
UCLASS(ClassGroup=SkillCreator, meta=(BlueprintSpawnableComponent))
class SKILLCREATORRUNTIME_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UInventoryComponent();

    static constexpr int32 HotbarSize = 10;
    static constexpr int32 TotalSize  = 30;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
    TArray<FItemStack> Slots;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
    int32 ActiveHotbarIndex = 0;

    // 查詢
    UFUNCTION(BlueprintPure, Category="Inventory")
    FItemStack GetActiveItem() const;

    UFUNCTION(BlueprintPure, Category="Inventory")
    int32 GetActiveToolTier() const;

    UFUNCTION(BlueprintPure, Category="Inventory")
    float GetActiveMiningSpeedMult() const;

    // 修改
    UFUNCTION(BlueprintCallable, Category="Inventory")
    int32 TryAdd(EItemId Id, int32 Count);

    UFUNCTION(BlueprintCallable, Category="Inventory")
    bool Consume(int32 SlotIndex, int32 Count = 1);

    UFUNCTION(BlueprintCallable, Category="Inventory")
    void SwapSlots(int32 A, int32 B);

    UFUNCTION(BlueprintCallable, Category="Inventory")
    void SetActiveHotbarIndex(int32 Idx);

protected:
    virtual void BeginPlay() override;
};
