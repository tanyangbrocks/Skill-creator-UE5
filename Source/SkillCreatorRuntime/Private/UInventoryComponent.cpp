#include "UInventoryComponent.h"
#include "ItemRegistry.h"
#include "MaterialRegistry.h"

// 2026-06-23：工具類別 vs 材質類別比對表（docs/plan-item-crafting-system.md §四：
// 木鏟只加速挖土／木斧只加速木製品／木鎬加速石頭跟礦物兩類）
static bool ToolCategoryMatchesMaterial(EToolCategory Tool, EMaterialCategory Mat)
{
    switch (Tool)
    {
    case EToolCategory::Shovel:  return Mat == EMaterialCategory::Soil;
    case EToolCategory::Axe:     return Mat == EMaterialCategory::Wood;
    case EToolCategory::Pickaxe: return Mat == EMaterialCategory::Stone || Mat == EMaterialCategory::Ore;
    default:                     return false;
    }
}

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    Slots.SetNum(TotalSize);
}

void UInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
    if (Slots.Num() != TotalSize) Slots.SetNum(TotalSize);
}

FItemStack UInventoryComponent::GetActiveItem() const
{
    return Slots.IsValidIndex(ActiveHotbarIndex) ? Slots[ActiveHotbarIndex] : FItemStack::Empty();
}

int32 UInventoryComponent::GetActiveToolTier() const
{
    FItemStack Active = GetActiveItem();
    if (Active.IsEmpty()) return 0;
    const FItemData& D = FItemRegistry::Get(Active.ItemId);
    return D.bIsTool ? D.ToolTier : 0;
}

float UInventoryComponent::GetActiveMiningSpeedMult(EMaterialType TargetMat) const
{
    FItemStack Active = GetActiveItem();
    if (Active.IsEmpty()) return 1.f;
    const FItemData& D = FItemRegistry::Get(Active.ItemId);

    // ToolCategory::None（一般工具，如基礎鎬/鐵鎬）：沿用舊行為，全域固定速度倍率
    if (D.ToolCategory == EToolCategory::None || TargetMat == EMaterialType::Air)
        return D.MiningSpeedMult;

    // 類別化工具（木鏟/木斧/石鎬等）：只有材質類別匹配才加成，不匹配仍可挖但無加速
    const EMaterialCategory MatCat = FMaterialRegistry::GetCategory(TargetMat);
    return ToolCategoryMatchesMaterial(D.ToolCategory, MatCat) ? D.MiningSpeedMult : 1.f;
}

int32 UInventoryComponent::TryAdd(EItemId Id, int32 Count)
{
    if (Id == EItemId::None || Count <= 0) return 0;

    int32 MaxStack  = FItemRegistry::Get(Id).MaxStack;
    int32 Remaining = Count;

    // 第一輪：填補同種物品已有格子
    for (int32 i = 0; i < TotalSize && Remaining > 0; ++i)
    {
        if (Slots[i].ItemId != Id) continue;
        int32 CanAdd = MaxStack - Slots[i].Count;
        if (CanAdd <= 0) continue;
        int32 Add = FMath::Min(CanAdd, Remaining);
        Slots[i] = Slots[i].WithCount(Slots[i].Count + Add);
        Remaining -= Add;
    }

    // 第二輪：填入空格
    for (int32 i = 0; i < TotalSize && Remaining > 0; ++i)
    {
        if (!Slots[i].IsEmpty()) continue;
        int32 Add = FMath::Min(MaxStack, Remaining);
        Slots[i] = FItemStack(Id, Add);
        Remaining -= Add;
    }

    return Count - Remaining;
}

bool UInventoryComponent::Consume(int32 SlotIndex, int32 Count)
{
    if (!Slots.IsValidIndex(SlotIndex)) return false;
    FItemStack& S = Slots[SlotIndex];
    if (S.IsEmpty() || S.Count < Count) return false;
    S = (S.Count == Count) ? FItemStack::Empty() : S.WithCount(S.Count - Count);
    return true;
}

void UInventoryComponent::SwapSlots(int32 A, int32 B)
{
    if (!Slots.IsValidIndex(A) || !Slots.IsValidIndex(B) || A == B) return;
    Swap(Slots[A], Slots[B]);
}

void UInventoryComponent::SetActiveHotbarIndex(int32 Idx)
{
    ActiveHotbarIndex = FMath::Clamp(Idx, 0, HotbarSize - 1);
}
