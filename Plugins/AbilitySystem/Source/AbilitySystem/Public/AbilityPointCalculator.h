#pragma once
#include "CoreMinimal.h"
#include "SpellArray.h"

// 能力點與 MP 費用計算器（對應 Godot AbilityPointCalculator.cs）。
// 純靜態工具類別；不依賴 UObject。
struct FAbilityPointCalculator
{
    // ── MP 乘數（依發動類型）────────────────────────────────────────
    // Instant=0.8 / Declare=1.0 / Sustained=1.5 / None=1.0
    static float GetMpMultiplier(EAbilityActivationType ActivationType)
    {
        switch (ActivationType)
        {
            case EAbilityActivationType::Instant:   return 0.8f;
            case EAbilityActivationType::Declare:   return 1.0f;
            case EAbilityActivationType::Sustained: return 1.5f;
            default:                                return 1.0f;
        }
    }

    // ── 雙曲線縮放：f(x) = 1 - 1 / (1 + a*x)（傷害/概率類刻印）──
    static float HyperbolicEffect(float Points, float A)
    {
        return 1.f - 1.f / (1.f + A * Points);
    }

    // ── 線性縮放：f(x) = Base + x * K（輔助類刻印）────────────────
    static float LinearEffect(float Points, float Base, float K)
    {
        return Base + Points * K;
    }

    // ── 計算技能整構的最終 MP 消耗（BaseMpCost × ActivationType 乘數）
    static float CalculateMpCost(const FSpellArray& Spell)
    {
        return Spell.BaseMpCost * GetMpMultiplier(Spell.ActivationType);
    }

    // ── 按 ManaTypeKey 分配 MP 費用（各類型佔非空非被動插槽的比例）─
    // 回傳：ManaTypeKey → 該類型應分攤的 MP 量
    static TMap<FName, float> CalculateSlotCostByType(const FSpellArray& Spell)
    {
        TMap<FName, float> Result;
        const float TotalMp = CalculateMpCost(Spell);

        TMap<FName, int32> CountByType;
        int32 TotalBound = 0;

        for (const FSpellSlot& Slot : Spell.Slots)
        {
            if (Slot.IsEmpty()) continue;
            if (Slot.TotemType == ETotemType::Passive) continue;
            if (Slot.ManaTypeKey.IsNone()) continue;
            CountByType.FindOrAdd(Slot.ManaTypeKey, 0)++;
            ++TotalBound;
        }

        if (TotalBound == 0) return Result;

        for (auto& [Key, Count] : CountByType)
            Result.Add(Key, TotalMp * (float)Count / (float)TotalBound);

        return Result;
    }

    // ── 等級 → 能力點上限（W-10，對應 Godot PlayerController.TierApCap）──
    static int32 TierApCap(int32 PlayerLevel)
    {
        if (PlayerLevel < 10)  return 50;
        if (PlayerLevel < 20)  return 120;
        if (PlayerLevel < 35)  return 200;
        if (PlayerLevel < 50)  return 350;
        if (PlayerLevel < 65)  return 500;
        if (PlayerLevel < 80)  return 700;
        if (PlayerLevel < 100) return 900;
        return 1500;
    }

    // ── 計算技能整構能力點總消耗（W-6）────────────────────────────
    // = Σ(slot.AbilityPointCost + Σslot.LocalEngravings[i].Points) + Σ GlobalEngravings[i].Points
    static int32 CalculateTotalCost(const FSpellArray& Spell)
    {
        int32 Total = 0;
        for (const FSpellSlot& Slot : Spell.Slots)
        {
            if (Slot.IsEmpty()) continue;
            Total += Slot.AbilityPointCost;
            for (const FEngraveData& E : Slot.LocalEngravings)
                Total += E.Points;
        }
        for (const FEngraveData& E : Spell.GlobalEngravings)
            Total += E.Points;
        return Total;
    }

    static bool ExceedsLevelCap(const FSpellArray& Spell, int32 PlayerLevel)
    {
        return CalculateTotalCost(Spell) > TierApCap(PlayerLevel);
    }
};
