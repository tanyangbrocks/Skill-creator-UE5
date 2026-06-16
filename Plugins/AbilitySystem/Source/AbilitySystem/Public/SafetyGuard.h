#pragma once
#include "CoreMinimal.h"
#include "ManaType.h"  // EAbilityActivationType

// 施法安全限制器（對應 Godot SafetyGuard.cs）。
// 防止無限循環、proc 濫用、場景唯一次刻印超限。
// 每個 FSpellRunner::FActiveEntry 持有一個 FSafetyGuard 實例。
struct FSafetyGuard
{
    // ── 全域常數 ────────────────────────────────────────────────────
    static constexpr int32 MaxExecutionsPerTick = 50;   // 每 tick 最大執行步數
    static constexpr int32 MaxWhileIterations   = 500;  // RepeatWhile 每次最大迭代
    static constexpr int32 MaxComboDepth        = 5;    // InvokeSpell 連段最大深度
    static constexpr int32 MaxStepsPerCast      = 300;  // 單次施放最大步數（含等待）
    static constexpr int32 MaxEntityCount       = 100;  // 單次施放最大影響實體數
    static constexpr int32 MaxContainerDepth    = 3;    // 容器效果最大巢狀深度

    // ── MP 門檻 ─────────────────────────────────────────────────────
    static bool HasMp(float CurrentMp, float Cost) { return CurrentMp >= Cost; }

    // ── Proc Mask（同一連段中每個刻印最多觸發一次）────────────────
    // TryProc：首次呼叫回傳 true 並記錄；重複呼叫回傳 false
    bool TryProc(FName EngraveId)
    {
        bool bAlreadySet = false;
        ProcMask.Add(EngraveId, &bAlreadySet);
        return !bAlreadySet;
    }
    void ResetProcMask() { ProcMask.Reset(); }

    // ── 場景唯一次刻印計數器（跨整個場景，不隨施放重置）──────────
    // TryUseSpell：Limit=0 代表無限制；超過上限回傳 false
    bool TryUseSpell(FName SpellName, int32 Limit)
    {
        if (Limit <= 0) return true;
        int32& Used = SceneUseCounts.FindOrAdd(SpellName, 0);
        if (Used >= Limit) return false;
        ++Used;
        return true;
    }
    void ResetSceneCounts() { SceneUseCounts.Reset(); }

private:
    TSet<FName>       ProcMask;        // 本連段已觸發的刻印
    TMap<FName, int32> SceneUseCounts; // 本場景各技能已施放次數
};
