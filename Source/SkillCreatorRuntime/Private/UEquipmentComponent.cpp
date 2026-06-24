#include "UEquipmentComponent.h"
#include "UInventoryComponent.h"
#include "ItemRegistry.h"

UEquipmentComponent::UEquipmentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

EItemId UEquipmentComponent::GetEquipped(FName SlotId) const
{
    const EItemId* Found = EquippedBySlot.Find(SlotId);
    return Found ? *Found : EItemId::None;
}

float UEquipmentComponent::GetTotalDefFlat() const
{
    float Total = 0.f;
    for (const auto& Pair : EquippedBySlot)
        if (Pair.Value != EItemId::None)
            Total += FItemRegistry::Get(Pair.Value).DefFlat;
    return Total;
}

float UEquipmentComponent::GetTotalMpBonus() const
{
    float Total = 0.f;
    for (const auto& Pair : EquippedBySlot)
        if (Pair.Value != EItemId::None)
            Total += FItemRegistry::Get(Pair.Value).MpBonus;
    return Total;
}

bool UEquipmentComponent::TryEquip(UInventoryComponent* Inv, int32 HotbarIndex)
{
    if (!Inv || !Inv->Slots.IsValidIndex(HotbarIndex)) return false;
    const FItemStack& Stack = Inv->Slots[HotbarIndex];
    if (Stack.IsEmpty()) return false;

    const FItemData& Data = FItemRegistry::Get(Stack.ItemId);
    if (Data.EquipSlot.IsNone()) return false;

    EItemId OldId = GetEquipped(Data.EquipSlot);

    Inv->Consume(HotbarIndex, 1);
    if (OldId != EItemId::None) Inv->TryAdd(OldId, 1);

    EquippedBySlot.Add(Data.EquipSlot, Stack.ItemId);
    return true;
}

bool UEquipmentComponent::TryUnequip(UInventoryComponent* Inv, FName SlotId)
{
    EItemId Equipped = GetEquipped(SlotId);
    if (Equipped == EItemId::None) return false;

    if (Inv) Inv->TryAdd(Equipped, 1);
    EquippedBySlot.Add(SlotId, EItemId::None);
    return true;
}
