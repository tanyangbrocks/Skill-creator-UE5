#include "SpellDescriptionGenerator.h"
#include "SpellArray.h"
#include "ElementType.h"

// ─── 列舉名稱映射 ────────────────────────────────────────────────────────────

FString FSpellDescriptionGenerator::ColorName(uint8 C)
{
    switch ((EEngraveColor)C)
    {
    case EEngraveColor::Action:    return TEXT("行動");
    case EEngraveColor::Black:     return TEXT("黑·邏輯");
    case EEngraveColor::White:     return TEXT("白·傷害");
    case EEngraveColor::Orange:    return TEXT("橙·控制");
    case EEngraveColor::Blue:      return TEXT("藍·改造");
    case EEngraveColor::Red:       return TEXT("紅·侵略");
    case EEngraveColor::Green:     return TEXT("綠·輔助");
    case EEngraveColor::Purple:    return TEXT("紫·額外");
    case EEngraveColor::Yellow:    return TEXT("黃·限制");
    case EEngraveColor::Elemental: return TEXT("元素");
    case EEngraveColor::Law:       return TEXT("法則");
    default:                       return TEXT("未知");
    }
}

FString FSpellDescriptionGenerator::TriggerName(uint8 T)
{
    switch ((EEngraveTrigger)T)
    {
    case EEngraveTrigger::OnCast: return TEXT("施放時");
    case EEngraveTrigger::OnTick: return TEXT("每幀");
    case EEngraveTrigger::OnHit:  return TEXT("命中時");
    default:                      return TEXT("無");
    }
}

FString FSpellDescriptionGenerator::ContainerName(uint8 C)
{
    switch ((EContainerType)C)
    {
    case EContainerType::DirectCast: return TEXT("直接施放");
    case EContainerType::Projectile: return TEXT("投射物");
    case EContainerType::Contact:    return TEXT("接觸命中");
    case EContainerType::Summon:     return TEXT("召喚物");
    case EContainerType::Area:       return TEXT("區域");
    default:                         return TEXT("未知");
    }
}

FString FSpellDescriptionGenerator::TotemTypeName(uint8 T)
{
    switch ((ETotemType)T)
    {
    case ETotemType::Area:         return TEXT("範圍");
    case ETotemType::Technique:    return TEXT("武技");
    case ETotemType::Projectile:   return TEXT("投射物");
    case ETotemType::Passive:      return TEXT("被動");
    case ETotemType::Morph:        return TEXT("變幻");
    case ETotemType::Displacement: return TEXT("位移");
    case ETotemType::Summon:       return TEXT("召喚");
    case ETotemType::Domain:       return TEXT("領域");
    default:                       return TEXT("自定義");
    }
}

FString FSpellDescriptionGenerator::ActivationName(uint8 A)
{
    // EAbilityActivationType is in ManaType.h
    switch (A)
    {
    case 0:  return TEXT("無");
    case 1:  return TEXT("即時");
    case 2:  return TEXT("蓄力");
    case 3:  return TEXT("持續");
    case 4:  return TEXT("宣告");
    default: return TEXT("未知");
    }
}

// ─── 輔助工具 ────────────────────────────────────────────────────────────────

FString FSpellDescriptionGenerator::DescribeEngraving(const FEngraveData& Eng, bool bShort)
{
    FString S = FString::Printf(TEXT("%s [%s|%s]"),
        *Eng.EngraveId.ToString(),
        *ColorName((uint8)Eng.Color),
        *TriggerName((uint8)Eng.Trigger));
    if (!bShort)
        S += FString::Printf(TEXT(" 效果:%.1f pts:%d"), Eng.CalculateEffect(), Eng.Points);
    return S;
}

FString FSpellDescriptionGenerator::DescribeSlot(const FSpellSlot& Slot, int32 Idx)
{
    if (Slot.IsEmpty())
        return FString::Printf(TEXT("插槽[%d] 空"), Idx);

    FString S = FString::Printf(TEXT("插槽[%d] %s·%s"),
        Idx,
        *Slot.TotemId.ToString(),
        *TotemTypeName((uint8)Slot.TotemType));

    if (!Slot.ManaTypeKey.IsNone())
        S += FString::Printf(TEXT(" MP:%s"), *Slot.ManaTypeKey.ToString());
    if (Slot.AbilityPointCost > 0)
        S += FString::Printf(TEXT(" AP:%d"), Slot.AbilityPointCost);

    for (const FEngraveData& E : Slot.LocalEngravings)
        S += TEXT("\n  ↳ ") + DescribeEngraving(E, true);

    return S;
}

// ─── 主要 API ────────────────────────────────────────────────────────────────

FString FSpellDescriptionGenerator::GenerateStructured(const FSpellArray& Spell)
{
    FString Out;
    Out += FString::Printf(TEXT("【%s】\n"), Spell.Name.IsEmpty() ? TEXT("（未命名）") : *Spell.Name);
    Out += FString::Printf(TEXT("施放: %s | 元素: %s | 延遲: %.1fs | 基礎消耗: %.0f MP\n"),
        *ContainerName((uint8)Spell.Container),
        *ElementName((uint8)Spell.SpellElement),
        Spell.CastDelay,
        Spell.BaseMpCost);

    if (!Spell.NextInCombo.IsEmpty())
        Out += FString::Printf(TEXT("連段 → %s\n"), *Spell.NextInCombo);
    if (Spell.SceneUseLimit > 0)
        Out += FString::Printf(TEXT("場景次數上限: %d\n"), Spell.SceneUseLimit);

    Out += TEXT("─────────────────\n");
    for (int32 i = 0; i < Spell.Slots.Num(); ++i)
        Out += DescribeSlot(Spell.Slots[i], i) + TEXT("\n");

    if (Spell.GlobalEngravings.Num() > 0)
    {
        Out += TEXT("─ 全局刻印 ─\n");
        for (const FEngraveData& E : Spell.GlobalEngravings)
            Out += TEXT("  ") + DescribeEngraving(E) + TEXT("\n");
    }

    if (Spell.ContainerEffect.IsValid())
    {
        Out += TEXT("─ 容器效果 ─\n");
        Out += GenerateStructured(*Spell.ContainerEffect);
    }

    return Out;
}

FString FSpellDescriptionGenerator::GenerateProse(const FSpellArray& Spell)
{
    const FString Name = Spell.Name.IsEmpty() ? TEXT("這個技能") : FString::Printf(TEXT("「%s」"), *Spell.Name);

    // 施法方式描述
    FString Desc = Name + TEXT("是一個") + ContainerName((uint8)Spell.Container) + TEXT("型技能");
    if (Spell.SpellElement != ESkillElementType::None)
        Desc += TEXT("，元素屬性為") + ElementName((uint8)Spell.SpellElement);
    if (Spell.CastDelay > 0.f)
        Desc += FString::Printf(TEXT("，施放延遲 %.1f 秒"), Spell.CastDelay);
    Desc += TEXT("。");

    // 插槽描述（非空插槽）
    TArray<FString> SlotDescs;
    for (const FSpellSlot& S : Spell.Slots)
    {
        if (S.IsEmpty()) continue;
        FString SD = TEXT("透過") + TotemTypeName((uint8)S.TotemType) + TEXT("因子");
        if (!S.ManaTypeKey.IsNone())
            SD += TEXT("（") + S.ManaTypeKey.ToString() + TEXT("法力）");
        SlotDescs.Add(SD);
    }
    if (SlotDescs.Num() > 0)
    {
        Desc += TEXT("此技能");
        for (int32 i = 0; i < SlotDescs.Num(); ++i)
        {
            Desc += SlotDescs[i];
            if (i < SlotDescs.Num() - 1) Desc += TEXT("，並");
        }
        Desc += TEXT("。");
    }

    // 刻印描述
    int32 TotalEngravings = Spell.GlobalEngravings.Num();
    for (const FSpellSlot& S : Spell.Slots)
        TotalEngravings += S.LocalEngravings.Num();

    if (TotalEngravings > 0)
        Desc += FString::Printf(TEXT("共裝備 %d 個刻印強化效果。"), TotalEngravings);

    // 連段
    if (!Spell.NextInCombo.IsEmpty())
        Desc += TEXT("施放後可接續「") + Spell.NextInCombo + TEXT("」進行連段。");

    return Desc;
}

FString FSpellDescriptionGenerator::ElementName(uint8 E)
{
    switch ((ESkillElementType)E)
    {
    case ESkillElementType::None:    return TEXT("無");
    case ESkillElementType::Metal:   return TEXT("金");
    case ESkillElementType::Wood:    return TEXT("木");
    case ESkillElementType::Water:   return TEXT("水");
    case ESkillElementType::Fire:    return TEXT("火");
    case ESkillElementType::Earth:   return TEXT("土");
    case ESkillElementType::Ice:     return TEXT("冰");
    case ESkillElementType::Wind:    return TEXT("風");
    case ESkillElementType::Light:   return TEXT("光");
    case ESkillElementType::Dark:    return TEXT("暗");
    case ESkillElementType::Thunder: return TEXT("雷");
    case ESkillElementType::Poison:  return TEXT("毒");
    default:                         return TEXT("未知");
    }
}
