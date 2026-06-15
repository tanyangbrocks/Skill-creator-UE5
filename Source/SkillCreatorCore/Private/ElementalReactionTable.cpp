#include "ElementalReactionTable.h"

// 正規化 key：小值在高 8 位，確保 A+B == B+A
static uint32 MakeKey(ESkillElementType A, ESkillElementType B)
{
    uint8 a = static_cast<uint8>(A);
    uint8 b = static_cast<uint8>(B);
    if (a > b) Swap(a, b);
    return (static_cast<uint32>(a) << 8) | b;
}

static const TMap<uint32, FReactionDef>& GetTable()
{
    static TMap<uint32, FReactionDef> T = []()
    {
        TMap<uint32, FReactionDef> Tab;
        using E = ESkillElementType;

        // ── 水系反應（5 條有元素狀態效果）────────────────────────────────
        Tab.Add(MakeKey(E::Water,   E::Metal),   { TEXT("鏽化"),
            []{ return MakeUnique<FRustEffect>();          } });
        Tab.Add(MakeKey(E::Water,   E::Wood),    { TEXT("蔓生"),
            []{ return MakeUnique<FGrowthSlowEffect>();    } });
        Tab.Add(MakeKey(E::Water,   E::Earth),   { TEXT("流沙"),
            []{ return MakeUnique<FQuicksandSlowEffect>(); } });
        Tab.Add(MakeKey(E::Water,   E::Thunder), { TEXT("感電"),
            []{ return MakeUnique<FElectrocutionEffect>(); } });
        Tab.Add(MakeKey(E::Water,   E::Ice),     { TEXT("結凍"),
            []{ return MakeUnique<FFrozenEffect>();        } });
        Tab.Add(MakeKey(E::Water,   E::Wind),    { TEXT("濃霧"),  nullptr });

        // ── 火系反應（W-3b CA 效果待填）──────────────────────────────────
        Tab.Add(MakeKey(E::Fire,    E::Metal),   { TEXT("熔爐"),  nullptr });
        Tab.Add(MakeKey(E::Fire,    E::Wood),    { TEXT("燃燒"),  nullptr });
        Tab.Add(MakeKey(E::Fire,    E::Water),   { TEXT("沸騰"),  nullptr });
        Tab.Add(MakeKey(E::Fire,    E::Ice),     { TEXT("融化"),  nullptr });
        Tab.Add(MakeKey(E::Fire,    E::Wind),    { TEXT("擴散"),  nullptr });
        Tab.Add(MakeKey(E::Fire,    E::Thunder), { TEXT("雷爆"),  nullptr });

        // ── 土系反應 ──────────────────────────────────────────────────────
        Tab.Add(MakeKey(E::Earth,   E::Metal),   { TEXT("強磁"),  nullptr });
        Tab.Add(MakeKey(E::Earth,   E::Wood),    { TEXT("紮根"),  nullptr });
        Tab.Add(MakeKey(E::Earth,   E::Wind),    { TEXT("沙塵"),  nullptr });

        // ── 雷系反應 ──────────────────────────────────────────────────────
        Tab.Add(MakeKey(E::Thunder, E::Metal),   { TEXT("超載"),  nullptr });
        Tab.Add(MakeKey(E::Ice,     E::Thunder), { TEXT("超導"),  nullptr });

        // ── 光 / 暗 / 毒系反應 ────────────────────────────────────────────
        Tab.Add(MakeKey(E::Light,   E::Metal),   { TEXT("幻象"),  nullptr });
        Tab.Add(MakeKey(E::Light,   E::Thunder), { TEXT("閃耀"),  nullptr });
        Tab.Add(MakeKey(E::Dark,    E::Light),   { TEXT("調和"),  nullptr });
        Tab.Add(MakeKey(E::Poison,  E::Wood),    { TEXT("腐化"),  nullptr });
        Tab.Add(MakeKey(E::Poison,  E::Dark),    { TEXT("凋零"),  nullptr });

        return Tab;
    }();
    return T;
}

const FReactionDef* FElementalReactionTable::Lookup(ESkillElementType A, ESkillElementType B)
{
    return GetTable().Find(MakeKey(A, B));
}
