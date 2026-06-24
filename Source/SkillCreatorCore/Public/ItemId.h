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

    // ── 武器（對應 origin text setting/base item system.txt §武器）─────
    WeaponWoodSword,
    WeaponStoneSword,

    // ── 工具（補上木系全套；ToolBasicAxe/ToolBasicPick 沿用既有 ID 對齊木斧/木鎬語義，不重複新增）──
    ToolWoodShovel,
    ToolStonePick,

    // ── 裝備（補滿 5 欄：頭盔/鎧甲/褲子/鞋子/飾品；EquipLeatherArmor/EquipAmulet 沿用既有 ID）──
    EquipLeatherHelmet,
    EquipLeatherPants,
    EquipLeatherBoots,

    // ── 素材（木系半成品）────────────────────────────────────────
    MaterialPlank,
    MaterialStick,

    // ── 可放置物（不可塑形，spawn Actor）─────────────────────────
    PlaceableWoodChest,
    PlaceableWoodWorkbench,

    // ── 道具（消耗品）────────────────────────────────────────────
    ConsumableBerry,

    // ── 樹木產物（W-A）───────────────────────────────────────────
    OakLog,         // 橡木原木（樹幹掉落，可放置為 Wood tile，可作加工素材）
    FallenLeaf,     // 落葉（樹葉掉落，可放置為 FallenLeaf tile，可塑形）
    OakSapling,     // 橡木樹苗（樹葉機率掉落；random tick 後可生長為橡木）
    OakFruit,       // 橡木果實（樹葉機率掉落；效果待定）

    // ── 地表植物（W-G）───────────────────────────────────────────
    Weed,           // 雜草（草地長出的採集 Entity；0.5s 採集，掉落 1 根 Weed 物品）

    COUNT UMETA(Hidden),
};
