#include "EquipmentSlot.h"

void FEquipmentSlotRegistry::Init(TArray<FEquipmentSlotDef>& Out)
{
    auto Reg = [&](FName Id, FText Name, int32 SortOrder)
    {
        FEquipmentSlotDef D; D.Id = Id; D.DisplayName = Name; D.SortOrder = SortOrder;
        Out.Add(MoveTemp(D));
    };

    // 對應 origin text setting/base item system.txt §裝備：頭盔/鎧甲/褲子/鞋子/飾品五個欄位
    // （武器不在這五欄裡，武器是 FItemData.bIsWeapon 的熱鍵欄持握物，不走裝備欄）
    Reg(TEXT("Helmet"),    INVTEXT("頭盔"), 0);
    Reg(TEXT("Armor"),     INVTEXT("鎧甲"), 1);
    Reg(TEXT("Pants"),     INVTEXT("褲子"), 2);
    Reg(TEXT("Boots"),     INVTEXT("鞋子"), 3);
    Reg(TEXT("Accessory"), INVTEXT("飾品"), 4);
}

const TArray<FEquipmentSlotDef>& FEquipmentSlotRegistry::GetAll()
{
    static TArray<FEquipmentSlotDef> Table;
    if (Table.IsEmpty())
    {
        Init(Table);
        Table.Sort([](const FEquipmentSlotDef& A, const FEquipmentSlotDef& B) { return A.SortOrder < B.SortOrder; });
    }
    return Table;
}

const FEquipmentSlotDef* FEquipmentSlotRegistry::Find(FName Id)
{
    for (const FEquipmentSlotDef& D : GetAll())
        if (D.Id == Id) return &D;
    return nullptr;
}
