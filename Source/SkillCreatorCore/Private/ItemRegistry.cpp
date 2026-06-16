#include "ItemRegistry.h"

static FItemData MakeBlock(EItemId Id, FText Name, EMaterialType Mat)
{
    FItemData D; D.Id = Id; D.DisplayName = Name;
    D.bIsPlaceable = true; D.PlaceAs = Mat; D.MaxStack = 99999; return D;
}
static FItemData MakeTool(EItemId Id, FText Name, int32 Tier, float Speed)
{
    FItemData D; D.Id = Id; D.DisplayName = Name;
    D.bIsTool = true; D.ToolTier = Tier; D.MiningSpeedMult = Speed; D.MaxStack = 1; return D;
}
static FItemData MakeEquip(EItemId Id, FText Name,
    EEquipmentSlotType Slot, float Atk, float Def, float Mp)
{
    FItemData D; D.Id = Id; D.DisplayName = Name; D.MaxStack = 1;
    D.EquipSlot = Slot; D.AtkMult = Atk; D.DefFlat = Def; D.MpBonus = Mp; return D;
}
static FItemData MakeMat(EItemId Id, FText Name)
{
    FItemData D; D.Id = Id; D.DisplayName = Name; D.MaxStack = 99999; return D;
}

void FItemRegistry::Init(TArray<FItemData>& Out)
{
    const int32 Count = (int32)EItemId::COUNT;
    Out.SetNum(Count);

    auto Set = [&](FItemData D) { Out[(int32)D.Id] = MoveTemp(D); };

    // None 佔位
    Set(FItemData{});

    // 方塊
    Set(MakeBlock(EItemId::BlockDirt,  INVTEXT("泥土"),  EMaterialType::Dirt));
    Set(MakeBlock(EItemId::BlockStone, INVTEXT("圓石"),  EMaterialType::Stone));
    Set(MakeBlock(EItemId::BlockWood,  INVTEXT("木材"),  EMaterialType::Wood));
    Set(MakeBlock(EItemId::BlockSand,  INVTEXT("沙"),    EMaterialType::Sand));
    Set(MakeBlock(EItemId::BlockAsh,   INVTEXT("灰燼"),  EMaterialType::Ash));

    // 工具
    Set(MakeTool(EItemId::ToolBasicPick, INVTEXT("基礎鎬"), 1, 2.5f));
    Set(MakeTool(EItemId::ToolBasicAxe,  INVTEXT("基礎斧"), 0, 2.0f));
    Set(MakeTool(EItemId::ToolIronPick,  INVTEXT("鐵鎬"),   2, 3.5f));

    // 裝備
    Set(MakeEquip(EItemId::EquipBasicSword,   INVTEXT("基礎劍"),   EEquipmentSlotType::Weapon,    1.3f, 0.f, 0.f));
    Set(MakeEquip(EItemId::EquipLeatherArmor, INVTEXT("皮革護甲"), EEquipmentSlotType::Armor,     1.0f, 5.f, 0.f));
    Set(MakeEquip(EItemId::EquipAmulet,       INVTEXT("護符"),     EEquipmentSlotType::Accessory, 1.0f, 0.f, 30.f));

    // 礦石原材料
    Set(MakeMat(EItemId::OreCoal,         INVTEXT("煤炭")));
    Set(MakeMat(EItemId::OreCopperRaw,    INVTEXT("生銅礦")));
    Set(MakeMat(EItemId::OreIronRaw,      INVTEXT("生鐵礦")));
    Set(MakeMat(EItemId::OreMagicCrystal, INVTEXT("魔晶石")));

    // 材質碎片
    Set(MakeMat(EItemId::FragmentDirt,         INVTEXT("土塊碎片")));
    Set(MakeMat(EItemId::FragmentStone,        INVTEXT("石塊碎片")));
    Set(MakeMat(EItemId::FragmentSand,         INVTEXT("沙礫碎片")));
    Set(MakeMat(EItemId::FragmentWood,         INVTEXT("木屑碎片")));
    Set(MakeMat(EItemId::FragmentAsh,          INVTEXT("灰燼碎片")));
    Set(MakeMat(EItemId::FragmentCoal,         INVTEXT("煤炭碎片")));
    Set(MakeMat(EItemId::FragmentCopper,       INVTEXT("銅礦碎片")));
    Set(MakeMat(EItemId::FragmentIron,         INVTEXT("鐵礦碎片")));
    Set(MakeMat(EItemId::FragmentMagicCrystal, INVTEXT("魔晶碎片")));
}

const FItemData& FItemRegistry::Get(EItemId Id)
{
    static TArray<FItemData> Table;
    if (Table.IsEmpty()) Init(Table);
    int32 Idx = (int32)Id;
    return (Idx >= 0 && Idx < Table.Num()) ? Table[Idx] : Table[0];
}
