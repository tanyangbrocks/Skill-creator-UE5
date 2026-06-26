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
    FName Slot, float Atk, float Def, float Mp)
{
    FItemData D; D.Id = Id; D.DisplayName = Name; D.MaxStack = 1;
    D.EquipSlot = Slot; D.AtkMult = Atk; D.DefFlat = Def; D.MpBonus = Mp; return D;
}
static FItemData MakeMat(EItemId Id, FText Name)
{
    FItemData D; D.Id = Id; D.DisplayName = Name; D.MaxStack = 99999; return D;
}
static FItemData MakeWeapon(EItemId Id, FText Name, float AtkMult)
{
    FItemData D; D.Id = Id; D.DisplayName = Name; D.MaxStack = 1;
    D.bIsWeapon = true; D.AtkMult = AtkMult; return D;
}
static FItemData MakeConsumable(EItemId Id, FText Name)
{
    FItemData D; D.Id = Id; D.DisplayName = Name; D.MaxStack = 99;
    D.bIsConsumable = true; return D;
}
// 可塑形可放置物（吃形狀選單，PlaceAs=tile 材質）；素材兼具 bIsComponent=true（可用於加工）
static FItemData MakeMoldablePlaceable(EItemId Id, FText Name, EMaterialType Mat)
{
    FItemData D = MakeBlock(Id, Name, Mat);
    D.bIsMaterial = true; D.bIsComponent = true; return D;
}
// 不可塑形可放置物（固定 Actor，PlaceAsActor 留待對應 Actor 類別存在時——見物品系統 4——
// 在那邊用 FItemRegistry 的後置註冊補上，這裡先建好其餘欄位）
static FItemData MakeFixturePlaceable(EItemId Id, FText Name)
{
    FItemData D; D.Id = Id; D.DisplayName = Name; D.MaxStack = 1;
    D.bIsPlaceable = true; D.bIsComponent = true; return D;
}
// 工具（類別化：木鏟/木斧/木鎬/石鎬等只對匹配的材質類別加速，見 EToolCategory）
static FItemData MakeCategorizedTool(EItemId Id, FText Name, int32 Tier, float Speed, EToolCategory Cat)
{
    FItemData D = MakeTool(Id, Name, Tier, Speed);
    D.ToolCategory = Cat; return D;
}

void FItemRegistry::Init(TArray<FItemData>& Out)
{
    const int32 Count = (int32)EItemId::COUNT;
    Out.SetNum(Count);

    auto Set = [&](FItemData D) { Out[(int32)D.Id] = MoveTemp(D); };

    // None 佔位
    Set(FItemData{});

    // 方塊
    Set(MakeBlock(EItemId::BlockDirt,  INVTEXT("泥土"),  EMaterialType::Dirt_Dry));
    Set(MakeBlock(EItemId::BlockStone, INVTEXT("圓石"),  EMaterialType::Stone_Cobble));
    // BlockWood＝原木（2026-06-23 改名對齊規格，原值「木材」語意上應該是加工物 MaterialPlank；
    // 原木同時是加工系統的原素材，bIsMaterial/bIsComponent=true）
    Set(MakeMoldablePlaceable(EItemId::BlockWood, INVTEXT("原木"), EMaterialType::Wood));
    Set(MakeBlock(EItemId::BlockSand,  INVTEXT("沙"),    EMaterialType::Sand));
    Set(MakeBlock(EItemId::BlockAsh,   INVTEXT("灰燼"),  EMaterialType::Ash));

    // 工具（2026-06-23：ToolBasicPick/ToolBasicAxe 對齊規格「木鎬」「木斧」語義並補上 ToolCategory，
    // 不新增重複 ID，避免存檔相容性問題——見 docs/plan-item-crafting-system.md §九 開放問題 Q1）
    Set(MakeCategorizedTool(EItemId::ToolBasicPick, INVTEXT("木鎬"), 1, 2.5f, EToolCategory::Pickaxe));
    Set(MakeCategorizedTool(EItemId::ToolBasicAxe,  INVTEXT("木斧"), 0, 2.0f, EToolCategory::Axe));
    Set(MakeCategorizedTool(EItemId::ToolIronPick,  INVTEXT("鐵鎬"), 2, 3.5f, EToolCategory::Pickaxe));
    Set(MakeCategorizedTool(EItemId::ToolWoodShovel, INVTEXT("木鏟"), 0, 2.0f, EToolCategory::Shovel));
    Set(MakeCategorizedTool(EItemId::ToolStonePick,  INVTEXT("石鎬"), 1, 3.0f, EToolCategory::Pickaxe));

    // 裝備（5 欄：頭盔/鎧甲/褲子/鞋子/飾品，對應 FEquipmentSlotRegistry；武器不佔裝備欄）
    Set(MakeEquip(EItemId::EquipLeatherArmor,  INVTEXT("皮革護甲"), TEXT("Armor"),     1.0f, 5.f, 0.f));
    Set(MakeEquip(EItemId::EquipAmulet,        INVTEXT("護符"),     TEXT("Accessory"), 1.0f, 0.f, 30.f));
    Set(MakeEquip(EItemId::EquipLeatherHelmet, INVTEXT("皮革頭盔"), TEXT("Helmet"),    1.0f, 2.f, 0.f));
    Set(MakeEquip(EItemId::EquipLeatherPants,  INVTEXT("皮革褲子"), TEXT("Pants"),     1.0f, 2.f, 0.f));
    Set(MakeEquip(EItemId::EquipLeatherBoots,  INVTEXT("皮革鞋子"), TEXT("Boots"),     1.0f, 2.f, 0.f));

    // 武器（規格「★武器」：攻擊力 = 力量 × AtkMult，不佔裝備欄。
    // EquipBasicSword 原本佔用 Weapon 裝備欄，現改成跟新武器同一套 bIsWeapon 機制）
    Set(MakeWeapon(EItemId::EquipBasicSword,  INVTEXT("基礎劍"), 1.3f));
    {
        FItemData D = MakeWeapon(EItemId::WeaponWoodSword, INVTEXT("木劍"), 1.0f);
        D.MeshPath = FSoftObjectPath(TEXT("/Game/Weapons/DemonSword/SM_DemonSword.SM_DemonSword"));
        Set(MoveTemp(D));
    }
    Set(MakeWeapon(EItemId::WeaponStoneSword, INVTEXT("石劍"),   1.3f));

    // 素材（可塑形可放置物，吃形狀選單；同時 bIsComponent=true 可用於加工）
    Set(MakeMoldablePlaceable(EItemId::MaterialPlank, INVTEXT("木材"), EMaterialType::Wood));
    Set(MakeMat(EItemId::MaterialStick, INVTEXT("木棍")));  // 木棍本身不可放置，純加工素材

    // 可放置物（不可塑形：木寶箱/木工作臺，PlaceAsActor 在物品系統 4 補上對應 Actor 類別）
    Set(MakeFixturePlaceable(EItemId::PlaceableWoodChest,     INVTEXT("木寶箱")));
    Set(MakeFixturePlaceable(EItemId::PlaceableWoodWorkbench, INVTEXT("木工作臺")));

    // 道具（消耗品，左鍵使用）
    Set(MakeConsumable(EItemId::ConsumableBerry, INVTEXT("野莓")));

    // 樹木產物（W-A）
    Set(MakeMoldablePlaceable(EItemId::OakLog,    INVTEXT("橡木原木"), EMaterialType::Wood));
    Set(MakeMoldablePlaceable(EItemId::FallenLeaf,INVTEXT("落葉"),     EMaterialType::FallenLeaf));
    Set(MakeMat(EItemId::OakSapling, INVTEXT("橡木樹苗")));
    Set(MakeConsumable(EItemId::OakFruit, INVTEXT("橡木果實")));

    // 地表植物（W-G）
    Set(MakeMat(EItemId::Weed, INVTEXT("雜草")));  // Entity 採集掉落，不可放置

    // 礦石原材料（J-3：OreCoal/Copper/Iron/Magic 可放回地圖重建礦脈，Godot ItemRegistry.cs:41-44）
    Set(MakeBlock(EItemId::OreCoal,         INVTEXT("煤炭"),   EMaterialType::Ore_Coal));
    Set(MakeBlock(EItemId::OreCopperRaw,    INVTEXT("生銅礦"), EMaterialType::Ore_Copper));
    Set(MakeBlock(EItemId::OreIronRaw,      INVTEXT("生鐵礦"), EMaterialType::Ore_Iron));
    Set(MakeMat(EItemId::OreGoldRaw,        INVTEXT("生金礦")));  // UE5 擴充，無 Godot 對應，保持不可放置
    Set(MakeBlock(EItemId::OreMagicCrystal, INVTEXT("魔晶石"), EMaterialType::Ore_MagicCrystal));

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

TArray<FItemData>& FItemRegistry::GetMutableTable()
{
    static TArray<FItemData> Table;
    if (Table.IsEmpty()) Init(Table);
    return Table;
}

const FItemData& FItemRegistry::Get(EItemId Id)
{
    TArray<FItemData>& Table = GetMutableTable();
    int32 Idx = (int32)Id;
    return (Idx >= 0 && Idx < Table.Num()) ? Table[Idx] : Table[0];
}

void FItemRegistry::SetPlaceAsActor(EItemId Id, TSubclassOf<AActor> ActorClass)
{
    TArray<FItemData>& Table = GetMutableTable();
    int32 Idx = (int32)Id;
    if (Idx >= 0 && Idx < Table.Num())
    {
        Table[Idx].PlaceAsActor = ActorClass;
        Table[Idx].bIsPlaceable = true;
    }
}
