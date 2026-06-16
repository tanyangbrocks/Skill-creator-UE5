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

    // ── 是否超過等級對應的能力點上限（W-10 等級系統建立後補全）──
    // TierApCap：等級 → 能力點上限（暫定線性公式，W-10 確認後調整）
    static int32 TierApCap(int32 PlayerLevel)
    {
        return 50 + PlayerLevel * 10; // stub：W-10 確認後調整
    }

    // ── 計算技能整構能力點總消耗（W-6 刻印成本系統建立後補全）──────
    // Stub：FSpellSlot::AbilityPointCost 尚未從 FTotemData 填入（W-6 延遲）
    static int32 CalculateTotalCost(const FSpellArray& /*Spell*/)
    {
        return 0; // W-6：接 FSpellSlot.AbilityPointCost + FEngraveData.TotalAbilityPointCost
    }

    static bool ExceedsLevelCap(const FSpellArray& Spell, int32 PlayerLevel)
    {
        return CalculateTotalCost(Spell) > TierApCap(PlayerLevel);
    }
};
