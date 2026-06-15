#pragma once
#include "CoreMinimal.h"
#include "IElementalTarget.h"

// 元素狀態效果抽象基底（純 C++；對應 Godot ElementalStatusEffect.cs）
// 不用 UObject — 由 UElementalAuraComponent 以 TUniquePtr 管理生命週期。
class FElementalStatusEffect
{
public:
    virtual ~FElementalStatusEffect() = default;

    float RemainingDuration = 0.f;

    virtual FName GetEffectType()          const = 0;  // 型別識別（取代 RTTI）
    virtual int32 GetMaxStacks()           const = 0;
    virtual void  OnApply(IElementalTarget* Target) {}
    virtual void  OnProcess(float DeltaTime)        {}

    virtual float GetSpeedPenalty()     const { return 0.f;   }
    virtual bool  GetImmobilizes()      const { return false;  }
    virtual float GetDamageTakenBonus() const { return 0.f;   }
    virtual float GetDefensePenalty()   const { return 0.f;   }

    virtual float GetAccumulatedState()           const { return 0.f; }
    virtual void  RestoreAccumulatedState(float V)      {}
};

// ── 鏽化（水 + 金）：防禦 -10%，5 秒，最多 3 層 ──────────────────────
class FRustEffect : public FElementalStatusEffect
{
public:
    static constexpr float DefPen       = 0.10f;
    static constexpr float DefaultDur   = 5.0f;

    FName GetEffectType()      const override { return "Rust";   }
    int32 GetMaxStacks()       const override { return 3;         }
    float GetDefensePenalty()  const override { return DefPen;    }
};

// ── 蔓生（水 + 木）：移速 -15% / 層，4 秒，最多 5 層 ──────────────────
class FGrowthSlowEffect : public FElementalStatusEffect
{
public:
    static constexpr float SpdPen     = 0.15f;
    static constexpr float DefaultDur = 4.0f;

    FName GetEffectType()   const override { return "GrowthSlow"; }
    int32 GetMaxStacks()    const override { return 5;             }
    float GetSpeedPenalty() const override { return SpdPen;        }
};

// ── 流沙（水 + 土）：每秒累積移速 -10%，上限 80%，5 秒，最多 8 層 ────
class FQuicksandSlowEffect : public FElementalStatusEffect
{
public:
    static constexpr float SpdPerSec  = 0.10f;
    static constexpr float MaxSpdPen  = 0.80f;
    static constexpr float DefaultDur = 5.0f;

    FName GetEffectType()   const override { return "Quicksand"; }
    int32 GetMaxStacks()    const override { return 8;            }
    float GetSpeedPenalty() const override { return Current;      }

    void OnProcess(float DeltaTime) override
    {
        Current = FMath::Min(MaxSpdPen, Current + SpdPerSec * DeltaTime);
    }

    float GetAccumulatedState()            const override { return Current; }
    void  RestoreAccumulatedState(float V)       override { Current = V;    }

private:
    float Current = 0.f;
};

// ── 感電（水 + 雷）：即時 5 傷害 + 0.5 秒麻痺，最多 3 層 ─────────────
class FElectrocutionEffect : public FElementalStatusEffect
{
public:
    static constexpr float ContactDmg  = 5.0f;
    static constexpr float DefaultDur  = 0.5f;

    FName GetEffectType()  const override { return "Electrocution"; }
    int32 GetMaxStacks()   const override { return 3; }
    bool  GetImmobilizes() const override { return RemainingDuration > 0.f; }

    void OnApply(IElementalTarget* Target) override
    {
        if (Target) Target->TakeDirectDamage(ContactDmg);
    }
};

// ── 結凍（水 + 冰）：1 秒無法行動 + 受傷 +20%，最多 1 層（各層獨立計時）
class FFrozenEffect : public FElementalStatusEffect
{
public:
    static constexpr float DmgBonus   = 0.20f;
    static constexpr float DefaultDur = 1.0f;

    FName GetEffectType()       const override { return "Frozen";   }
    int32 GetMaxStacks()        const override { return 1;           }
    bool  GetImmobilizes()      const override { return RemainingDuration > 0.f; }
    float GetDamageTakenBonus() const override { return DmgBonus;    }
};
