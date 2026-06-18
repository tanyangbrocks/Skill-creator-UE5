#pragma once
#include "CoreMinimal.h"
#include "ItemId.generated.h"

// 物品 ID 列舉（對應 Godot ItemId.cs）
UENUM(BlueprintType)
enum class EItemId : uint8
{
    None = 0,

    // ── 方塊物品（可放置）──────────────────────────────────────────
    BlockDirt,
    BlockStone,
    BlockWood,
    BlockSand,
    BlockAsh,

    // ── 工具 ───────────────────────────────────────────────────────
    ToolBasicPick,
    ToolBasicAxe,
    ToolIronPick,

    // ── 裝備 ───────────────────────────────────────────────────────
    EquipBasicSword,
    EquipLeatherArmor,
    EquipAmulet,

    // ── 礦石原材料（W-4）──────────────────────────────────────────
    OreCoal,
    OreCopperRaw,
    OreIronRaw,
    OreGoldRaw,
    OreMagicCrystal,

    // ── 材質碎片（不可放置，合成原料）────────────────────────────
    FragmentDirt,
    FragmentStone,
    FragmentSand,
    FragmentWood,
    FragmentAsh,
    FragmentCoal,
    FragmentCopper,
    FragmentIron,
    FragmentMagicCrystal,

    COUNT UMETA(Hidden),
};
