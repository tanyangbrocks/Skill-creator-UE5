#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemId.h"
#include "EquipmentSlot.h"
#include "UEquipmentComponent.generated.h"

class UInventoryComponent;

// 裝備欄組件（對應 Godot PlayerEquipment.cs）。
// 2026-06-23：從固定欄位（WeaponId/ArmorId/AccessoryId）改成 TMap<FName,EItemId>，
// 欄位數量完全由 FEquipmentSlotRegistry 決定，新增欄位不需要改這個類別。
// 武器不再是裝備欄的一種（見 docs/plan-item-crafting-system.md §衝突2），
// GetTotalAtkMult() 改成查熱鍵欄目前選中物品，不查裝備欄。
UCLASS(ClassGroup=SkillCreator, meta=(BlueprintSpawnableComponent))
class SKILLCREATORRUNTIME_API UEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UEquipmentComponent();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment")
    TMap<FName, EItemId> EquippedBySlot;

    // 查詢單一欄位目前裝備的物品（未裝備回傳 EItemId::None）
    UFUNCTION(BlueprintPure, Category="Equipment")
    EItemId GetEquipped(FName SlotId) const;

    // ── 計算屬性（套用裝備加成；DefFlat/MpBonus 加總全部裝備欄）─────
    UFUNCTION(BlueprintPure, Category="Equipment") float GetTotalDefFlat()  const;
    UFUNCTION(BlueprintPure, Category="Equipment") float GetTotalMpBonus()  const;

    // 從背包熱鍵欄槽位裝備，舊裝備退回背包（依 FItemData.EquipSlot 決定要裝到哪個欄位）
    UFUNCTION(BlueprintCallable, Category="Equipment")
    bool TryEquip(UInventoryComponent* Inv, int32 HotbarIndex);

    // 卸下裝備退回背包
    UFUNCTION(BlueprintCallable, Category="Equipment")
    bool TryUnequip(UInventoryComponent* Inv, FName SlotId);
};
