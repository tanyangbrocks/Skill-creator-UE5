#include "ManaTypeRegistry.h"

FManaTypeRegistry& FManaTypeRegistry::Get()
{
    static FManaTypeRegistry Instance;
    return Instance;
}

FManaTypeRegistry::FManaTypeRegistry()
{
    // ── 修煉六道 ──────────────────────────────────────────────
    Register({ 1,  FName("wu_dao"),     FText::FromString(TEXT("武道")),   EManaRootGroup::Cultivation, false, 1  });
    Register({ 2,  FName("xian_dao"),   FText::FromString(TEXT("仙道")),   EManaRootGroup::Cultivation, false, 2  });
    Register({ 3,  FName("fa_dao"),     FText::FromString(TEXT("法道")),   EManaRootGroup::Cultivation, false, 3  });
    Register({ 4,  FName("yi_dao"),     FText::FromString(TEXT("意道")),   EManaRootGroup::Cultivation, false, 4  });
    Register({ 5,  FName("hun_dao"),    FText::FromString(TEXT("魂道")),   EManaRootGroup::Cultivation, false, 5  });
    Register({ 6,  FName("gui_dao"),    FText::FromString(TEXT("詭道")),   EManaRootGroup::Cultivation, false, 6  });

    // ── 支配六法 ──────────────────────────────────────────────
    Register({ 7,  FName("mo_fa"),      FText::FromString(TEXT("魔法")),   EManaRootGroup::Domination,  false, 7  });
    Register({ 8,  FName("yao_li"),     FText::FromString(TEXT("妖力")),   EManaRootGroup::Domination,  false, 8  });
    Register({ 9,  FName("ao_shu"),     FText::FromString(TEXT("奧術")),   EManaRootGroup::Domination,  false, 9  });
    Register({ 10, FName("shen_sheng"), FText::FromString(TEXT("神聖力")), EManaRootGroup::Domination,  false, 10 });
    Register({ 11, FName("yuan_neng"),  FText::FromString(TEXT("源能")),   EManaRootGroup::Domination,  false, 11 });
    Register({ 12, FName("xing_neng"),  FText::FromString(TEXT("星能")),   EManaRootGroup::Domination,  false, 12 });

    // ── 世界六意 ──────────────────────────────────────────────
    Register({ 13, FName("ji_neng"),    FText::FromString(TEXT("技能")),   EManaRootGroup::World,       false, 13 });
    Register({ 14, FName("zhi_ye"),     FText::FromString(TEXT("職業")),   EManaRootGroup::World,       false, 14 });
    Register({ 15, FName("chao_neng"),  FText::FromString(TEXT("超能")),   EManaRootGroup::World,       false, 15 });
    Register({ 16, FName("shen_li"),    FText::FromString(TEXT("神力")),   EManaRootGroup::World,       false, 16 });
    Register({ 17, FName("gai_nian"),   FText::FromString(TEXT("概念")),   EManaRootGroup::World,       false, 17 });
    Register({ 18, FName("xun_neng"),   FText::FromString(TEXT("尋能")),   EManaRootGroup::World,       false, 18 });
}

void FManaTypeRegistry::Register(FManaType Type)
{
    All.Add(Type.Key, MoveTemp(Type));
}

const FManaType* FManaTypeRegistry::Find(FName Key) const
{
    return All.Find(Key);
}

bool FManaTypeRegistry::AreSameRoot(FName KeyA, FName KeyB) const
{
    const FManaType* A = All.Find(KeyA);
    const FManaType* B = All.Find(KeyB);
    return A && B && A->RootGroup == B->RootGroup;
}

TArray<const FManaType*> FManaTypeRegistry::GetSortedForHud() const
{
    TArray<const FManaType*> Result;
    for (const auto& Pair : All)
        Result.Add(&Pair.Value);
    Result.Sort([](const FManaType& A, const FManaType& B){ return A.SortOrder < B.SortOrder; });
    return Result;
}
