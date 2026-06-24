#include "NPCKind.h"

void FNPCKindRegistry::Init(TArray<FNPCSubtypeDefinition>& Out)
{
    auto Reg = [&](FName Id, ENPCGeneralKind Kind, FText Name, FText Desc, bool bImpl)
    {
        FNPCSubtypeDefinition D;
        D.Id = Id; D.Kind = Kind; D.DisplayName = Name; D.Description = Desc; D.bImplemented = bImpl;
        Out.Add(MoveTemp(D));
    };

    // 對應 origin text setting/base NPC system.txt §3 通用NPC，18 個子類逐條轉錄，
    // bImplemented 全部 false 除了游蕩詩人
    Reg(TEXT("CartMerchant"), ENPCGeneralKind::TravelingMerchant,
        INVTEXT("馬車商人"), INVTEXT("暫不實作"), false);
    Reg(TEXT("WalkingMerchant"), ENPCGeneralKind::TravelingMerchant,
        INVTEXT("步行商人"), INVTEXT("暫不實作"), false);

    Reg(TEXT("TreasureHunterTraveler"), ENPCGeneralKind::Traveler,
        INVTEXT("尋寶旅行者"), INVTEXT("暫不實作"), false);
    Reg(TEXT("PersonSeekerTraveler"), ENPCGeneralKind::Traveler,
        INVTEXT("尋人旅行者"), INVTEXT("暫不實作"), false);

    Reg(TEXT("Adventurer"), ENPCGeneralKind::Transcendent,
        INVTEXT("冒險者"), INVTEXT("暫不實作"), false);
    Reg(TEXT("GhostHunter"), ENPCGeneralKind::Transcendent,
        INVTEXT("獵鬼士"), INVTEXT("暫不實作"), false);
    Reg(TEXT("Hunter"), ENPCGeneralKind::Transcendent,
        INVTEXT("獵人"), INVTEXT("暫不實作"), false);

    Reg(TEXT("Thief"), ENPCGeneralKind::Criminal,
        INVTEXT("盜賊"), INVTEXT("暫不實作"), false);
    Reg(TEXT("WantedKiller"), ENPCGeneralKind::Criminal,
        INVTEXT("通緝犯殺人魔"), INVTEXT("暫不實作"), false);
    Reg(TEXT("Trafficker"), ENPCGeneralKind::Criminal,
        INVTEXT("人口販子"), INVTEXT("暫不實作"), false);

    Reg(TEXT("WanderingBard"), ENPCGeneralKind::Bard,
        INVTEXT("游蕩詩人"),
        INVTEXT("他可能出現在任何地方，做出任何行為，如果被玩家攻擊，會嘗試反擊"),
        true);

    Reg(TEXT("GatheringVillager"), ENPCGeneralKind::Villager,
        INVTEXT("採集村民"), INVTEXT("暫不實作"), false);
    Reg(TEXT("HuntingVillager"), ENPCGeneralKind::Villager,
        INVTEXT("狩獵村民"), INVTEXT("暫不實作"), false);

    Reg(TEXT("RefugeeChild"), ENPCGeneralKind::Refugee,
        INVTEXT("逃難孩童"), INVTEXT("暫不實作"), false);
    Reg(TEXT("Unconscious"), ENPCGeneralKind::Refugee,
        INVTEXT("昏迷者"), INVTEXT("暫不實作"), false);
}

const TArray<FNPCSubtypeDefinition>& FNPCKindRegistry::AllSubtypes()
{
    static TArray<FNPCSubtypeDefinition> Table;
    if (Table.IsEmpty()) Init(Table);
    return Table;
}

const FNPCSubtypeDefinition* FNPCKindRegistry::Find(FName SubtypeId)
{
    for (const FNPCSubtypeDefinition& D : AllSubtypes())
        if (D.Id == SubtypeId) return &D;
    return nullptr;
}
