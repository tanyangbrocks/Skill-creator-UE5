#pragma once
#include "CoreMinimal.h"

// ── Filter 條目 ─────────────────────────────────────────────────────
// 以 DeltaTime 累加計時（非 wall clock），遊戲暫停時自動停止。
// FilterType: "DamageShield" / "DeathGuard"
// Mode: filter-type 內部語義（"Reduce" / "Cancel" / "Once"）
struct FActionFilterEntry
{
    FName  FilterType;
    FName  Mode;
    bool   bOneShot  = false; // true = dispatch 後立刻移除（DeathGuard）
    float  Threshold = 0.f;   // DamageShield: duration sec（用於 Remaining 初值）
    float  CapValue  = 0.f;   // DamageShield: per-hit 傷害上限（0 = 全擋）
    float  Remaining = 0.f;   // >=0: DeltaTime 倒計時；<0: 不受時間限制（one-shot 用）
};

// ══════════════════════════════════════════════════════════════════════
//  FActionBus — 玩家受傷 / 死亡的攔截過濾管線
//
//  純 C++ struct（非 UObject）；持有者在 Tick() 呼叫 Update(DeltaTime)
//  讓計時自動跟隨遊戲暫停狀態（DeltaTime=0 時計時停止）。
//
//  典型用法（VM RegisterFilter opcode → SpellCaster → ActionBus）：
//    RegisterFilter("DamageShield", "Reduce", false, 5.f, 10.f)
//      → 5 秒內每次傷害至多 10 點
//    RegisterFilter("DeathGuard", "Once", true, 0.f, 0.f)
//      → 下一次致死傷害改為保留 1 HP
// ══════════════════════════════════════════════════════════════════════
struct SKILLCREATORCORE_API FActionBus
{
    // VM RegisterFilter opcode 呼叫（對應 ExecutionContext::RegisterFilterFn 簽名）
    void RegisterFilter(FName FilterType, FName Mode, bool bOneShot,
                        float Threshold, float CapValue);

    // 每幀遞減 Remaining；到期的項目移除
    void Update(float DeltaTime);

    // 玩家受傷攔截：回傳經過過濾後的傷害值
    float DispatchPlayerDamage(float InDamage);

    // 玩家死亡攔截：回傳 true = 死亡取消（DeathGuard 消耗）
    bool DispatchPlayerDeath();

private:
    TArray<FActionFilterEntry> Entries;
};
