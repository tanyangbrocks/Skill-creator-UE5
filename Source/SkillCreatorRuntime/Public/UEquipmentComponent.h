#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemId.h"
#include "EquipmentSlotType.h"
#include "UEquipmentComponent.generated.h"

class UInventoryComponent;

// 裝備欄組件（對應 Godot PlayerEquipment.cs）
// 持有武器/防具/飾品各一格；裝備時消耗背包物品，卸下時退回背包。
UCLASS(ClassGroup=SkillCreator, meta=(BlueprintSpawnableComponent))
class SKILLCREATORRUNTIME_API UEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UEquipmentComponent();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment")
    EItemId WeaponId    = EItemId::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment")
    EItemId ArmorId     = EItemId::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment")
    EItemId AccessoryId = EItemId::None;

    // ── 計算屬性（套用裝備加成）──────────────────────────────────
    UFUNCTION(BlueprintPure, Category="Equipment") float GetTotalAtkMult()  const;
    UFUNCTION(BlueprintPure, Category="Equipment") float GetTotalDefFlat()  const;
    UFUNCTION(BlueprintPure, Category="Equipment") float GetTotalMpBonus()  const;

    // 從背包熱鍵欄槽位裝備，舊裝備退回背包
    UFUNCTION(BlueprintCallable, Category="Equipment")
    bool TryEquip(UInventoryComponent* Inv, int32 HotbarIndex);

    // 卸下裝備退回背包
    UFUNCTION(BlueprintCallable, Category="Equipment")
    bool TryUnequip(UInventoryComponent* Inv, EEquipmentSlotType Slot);
};
