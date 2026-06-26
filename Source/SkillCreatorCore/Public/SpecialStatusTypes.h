#pragma once
#include "CoreMinimal.h"
#include "IElementalTarget.h"

// ── 特殊狀態類別 ────────────────────────────────────────────────────────────
// 所有可臨時賦予或移除的效果均屬「特殊狀態」，分三類：
//   Elemental — 元素 Aura 反應產生（由 UElementalAuraComponent 管理）
//   Abnormal  — 異常狀態（正面/負面，可疊加/不可疊加）
//   Other     — 其餘特殊狀態（腳本技能、環境特效等）
enum class ESpecialStatusCategory : uint8
{
    Elemental,
    Abnormal,
    Other,
};

// 異常狀態極性（正面 = 增益；負面 = 減益）
enum class EAbnormalPolarity : uint8
{
    Positive,
    Negative,
};

// ── 異常/其他特殊狀態基底（純 C++，不繼承 UObject）──────────────────────────
// 生命週期由 USpecialStatusComponent 以 TUniquePtr 管理。
// 具體狀態在子類中定義，架構上與 FElementalStatusEffect 對稱但獨立。
class FAbnormalStatusEffect
{
public:
    virtual ~FAbnormalStatusEffect() = default;

    // 剩餘持續時間（秒）；<=0 自動移除
    float RemainingDuration = 0.f;

    // ── Identity ──────────────────────────────────────────────────────────
    virtual FName              GetStatusId()    const = 0;
    virtual FText              GetDisplayName() const = 0;
    virtual ESpecialStatusCategory GetCategory()   const { return ESpecialStatusCategory::Abnormal; }
    virtual EAbnormalPolarity  GetPolarity()    const { return EAbnormalPolarity::Negative; }

    // ── 疊加策略 ──────────────────────────────────────────────────────────
    // IsStackable() == false: 同 Id 只允許 1 層，新層會刷新 RemainingDuration
    // IsStackable() == true:  允許最多 GetMaxStacks() 層各自獨立計時
    virtual bool  IsStackable()  const { return false; }
    virtual int32 GetMaxStacks() const { return 1; }

    // ── 生命週期回呼 ──────────────────────────────────────────────────────
    virtual void OnApply(IElementalTarget* Target)                    {}
    virtual void OnRemove(IElementalTarget* Target)                   {}
    virtual void OnProcess(float DeltaTime, IElementalTarget* Target) {}

    // ── 聚合數值修飾（負面：填正數；正面：見 Bonus/Has 欄）──────────────────
    virtual float GetSpeedPenalty()              const { return 0.f;  }
    virtual bool  GetImmobilizes()               const { return false; }  // 無法進行任何基礎動作
    virtual bool  GetCannotCastSkills()          const { return false; }  // 無法施放技能（含被動）
    virtual bool  GetCannotUseMP()               const { return false; }  // 所有 MP 鎖住
    virtual float GetDamageTakenBonus()          const { return 0.f;  }  // >0 = 多受傷
    virtual float GetDefensePenalty()            const { return 0.f;  }  // [0,1]；1 = 完全移除
    virtual float GetAttackPenalty()             const { return 0.f;  }  // >0 = 攻擊降低（負面）
    virtual float GetAttackSpeedPenalty()        const { return 0.f;  }  // >0 = 攻速降低
    virtual float GetLuckPenalty()               const { return 0.f;  }  // >0 = 幸運降低
    virtual float GetMiningSpeedPenalty()        const { return 0.f;  }  // >0 = 採掘速度降低
    virtual float GetAttackBonus()               const { return 0.f;  }  // >0 = 攻擊增強（正面）
    virtual float GetDefenseBonus()              const { return 0.f;  }  // >0 = 防禦增強（正面）
    // 正面狀態標旗
    virtual bool  GetHasSuperArmor()             const { return false; }  // 霸體：免疫行動封鎖
    virtual bool  GetIsEthereal()                const { return false; }  // 虛化：穿透
    virtual bool  GetIsInvincible()              const { return false; }  // 無敵：傷害歸零
    virtual bool  GetHasBasicElemResistance()    const { return false; }  // 基礎元素抵抗
    virtual bool  GetHasAdvancedElemResistance() const { return false; }  // 進階元素抵抗

    // ── 快照支援 ──────────────────────────────────────────────────────────
    virtual float GetAccumulatedState()            const { return 0.f; }
    virtual void  RestoreAccumulatedState(float V)       {}
};
