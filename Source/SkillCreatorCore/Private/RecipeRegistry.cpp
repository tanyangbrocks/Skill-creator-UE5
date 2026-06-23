#include "RecipeRegistry.h"

void FRecipeRegistry::Init(TArray<FRecipeEntry>& Out)
{
    auto Add = [&](EItemId Output, int32 OutCount, TArray<FRecipeInput> Inputs, bool bWorkbench)
    {
        FRecipeEntry E; E.Output = Output; E.OutputCount = OutCount;
        E.Inputs = MoveTemp(Inputs); E.bRequiresWorkbench = bWorkbench;
        Out.Add(MoveTemp(E));
    };
    auto In = [](EItemId Id, int32 Count) { FRecipeInput I; I.ItemId = Id; I.Count = Count; return I; };

    // 對應 origin text setting/base item system.txt §加工列表（規格原文「1根木棍、2個木材
    // 可加工為1把木劍」重複寫了兩次，已去重）
    Add(EItemId::MaterialPlank, 4, { In(EItemId::BlockWood, 1) }, false);
    Add(EItemId::MaterialStick, 4, { In(EItemId::MaterialPlank, 1) }, false);
    Add(EItemId::PlaceableWoodWorkbench, 1, { In(EItemId::MaterialPlank, 4) }, false);
    Add(EItemId::WeaponWoodSword, 1, { In(EItemId::MaterialStick, 1), In(EItemId::MaterialPlank, 2) }, true);
    Add(EItemId::ToolBasicAxe,    1, { In(EItemId::MaterialStick, 2), In(EItemId::MaterialPlank, 3) }, true);
    Add(EItemId::ToolBasicPick,   1, { In(EItemId::MaterialStick, 2), In(EItemId::MaterialPlank, 3) }, true);
    Add(EItemId::ToolWoodShovel,  1, { In(EItemId::MaterialStick, 2), In(EItemId::MaterialPlank, 1) }, true);
    Add(EItemId::PlaceableWoodChest, 1, { In(EItemId::MaterialPlank, 8) }, true);

    // 碎片合成原材質（100 碎片 → 1 方塊/礦石；plan-mining-placement.md §R-5）
    // 礦石類（Coal/Copper/Iron/MagicCrystal）對應 OreXxx，因為 EItemId 沒有獨立的 Block 版本
    Add(EItemId::BlockDirt,          1, { In(EItemId::FragmentDirt,         100) }, false);
    Add(EItemId::BlockStone,         1, { In(EItemId::FragmentStone,        100) }, false);
    Add(EItemId::BlockSand,          1, { In(EItemId::FragmentSand,         100) }, false);
    Add(EItemId::BlockWood,          1, { In(EItemId::FragmentWood,         100) }, false);
    Add(EItemId::BlockAsh,           1, { In(EItemId::FragmentAsh,          100) }, false);
    Add(EItemId::OreCoal,            1, { In(EItemId::FragmentCoal,         100) }, false);
    Add(EItemId::OreCopperRaw,       1, { In(EItemId::FragmentCopper,       100) }, false);
    Add(EItemId::OreIronRaw,         1, { In(EItemId::FragmentIron,         100) }, false);
    Add(EItemId::OreMagicCrystal,    1, { In(EItemId::FragmentMagicCrystal, 100) }, false);
}

const TArray<FRecipeEntry>& FRecipeRegistry::GetAll()
{
    static TArray<FRecipeEntry> Table;
    if (Table.IsEmpty()) Init(Table);
    return Table;
}
