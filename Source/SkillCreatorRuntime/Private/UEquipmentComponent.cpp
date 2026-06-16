#include "UEquipmentComponent.h"
#include "UInventoryComponent.h"
#include "ItemRegistry.h"

UEquipmentComponent::UEquipmentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

float UEquipmentComponent::GetTotalAtkMult() const
{
    return WeaponId    != EItemId::None ? FItemRegistry::Get(WeaponId).AtkMult    : 1.f;
}
float UEquipmentComponent::GetTotalDefFlat() const
{
    return ArmorId     != EItemId::None ? FItemRegistry::Get(ArmorId).DefFlat     : 0.f;
}
float UEquipmentComponent::GetTotalMpBonus() const
{
    return AccessoryId != EItemId::None ? FItemRegistry::Get(AccessoryId).MpBonus : 0.f;
}

bool UEquipmentComponent::TryEquip(UInventoryComponent* Inv, int32 HotbarIndex)
{
    if (!Inv || !Inv->Slots.IsValidIndex(HotbarIndex)) return false;
    const FItemStack& Stack = Inv->Slots[HotbarIndex];
    if (Stack.IsEmpty()) return false;

    const FItemData& Data = FItemRegistry::Get(Stack.ItemId);
    if (Data.EquipSlot == EEquipmentSlotType::None) return false;

    // 找出目前裝備的物品（準備退回背包）
    EItemId OldId = EItemId::None;
    switch (Data.EquipSlot)
    {
        case EEquipmentSlotType::Weapon:    OldId = WeaponId;    break;
        case EEquipmentSlotType::Armor:     OldId = ArmorId;     break;
        case EEquipmentSlotType::Accessory: OldId = AccessoryId; break;
        default: break;
    }

    Inv->Consume(HotbarIndex, 1);
    if (OldId != EItemId::None) Inv->TryAdd(OldId, 1);

    switch (Data.EquipSlot)
    {
        case EEquipmentSlotType::Weapon:    WeaponId    = Stack.ItemId; break;
        case EEquipmentSlotType::Armor:     ArmorId     = Stack.ItemId; break;
        case EEquipmentSlotType::Accessory: AccessoryId = Stack.ItemId; break;
        default: break;
    }
    return true;
}

bool UEquipmentComponent::TryUnequip(UInventoryComponent* Inv, EEquipmentSlotType Slot)
{
    EItemId Equipped = EItemId::None;
    switch (Slot)
    {
        case EEquipmentSlotType::Weapon:    Equipped = WeaponId;    break;
        case EEquipmentSlotType::Armor:     Equipped = ArmorId;     break;
        case EEquipmentSlotType::Accessory: Equipped = AccessoryId; break;
        default: break;
    }
    if (Equipped == EItemId::None) return false;

    if (Inv) Inv->TryAdd(Equipped, 1);

    switch (Slot)
    {
        case EEquipmentSlotType::Weapon:    WeaponId    = EItemId::None; break;
        case EEquipmentSlotType::Armor:     ArmorId     = EItemId::None; break;
        case EEquipmentSlotType::Accessory: AccessoryId = EItemId::None; break;
        default: break;
    }
    return true;
}
