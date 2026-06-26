#pragma once
#include "CoreMinimal.h"
#include "SpecialStatusTypes.h"
#include "ElementType.h"

// ── 全部異常狀態 ID 常數（含暫不實作的佔位）─────────────────────────────────
namespace AbnormalStatusId
{
    // 負面（已實作）
    static const FName Burn          = TEXT("Burn");
    static const FName Suffocation   = TEXT("Suffocation");
    static const FName Wet           = TEXT("Wet");
    static const FName Frostbite     = TEXT("Frostbite");
    static const FName Frozen        = TEXT("Frozen");
    static const FName Poison        = TEXT("Poison");
    static const FName DarkErosion   = TEXT("DarkErosion");
    static const FName InstantDeath  = TEXT("InstantDeath");
    static const FName Paralysis     = TEXT("Paralysis");
    static const FName EnergySeal    = TEXT("EnergySeal");
    static const FName SkillSeal     = TEXT("SkillSeal");
    static const FName Bleed         = TEXT("Bleed");
    static const FName Petrification = TEXT("Petrification");
    static const FName Dizzy         = TEXT("Dizzy");
    static const FName Cripple       = TEXT("Cripple");
    static const FName Fear          = TEXT("Fear");
    static const FName ExtremeFear   = TEXT("ExtremeFear");
    static const FName BadLuck       = TEXT("BadLuck");
    static const FName MiningFatigue = TEXT("MiningFatigue");
    // 正面（已實作）
    static const FName SuperArmor      = TEXT("SuperArmor");
    static const FName Ethereal        = TEXT("Ethereal");
    static const FName Invincible      = TEXT("Invincible");
    static const FName BasicElemResist = TEXT("BasicElemResist");
    static const FName AdvElemResist   = TEXT("AdvElemResist");
    // 佔位（暫不實作，無對應 class）
    static const FName Seal         = TEXT("Seal");          // 封印
    static const FName MoveBan      = TEXT("MoveBan");       // 移動禁制
    static const FName AttackBan    = TEXT("AttackBan");     // 攻擊禁制
    static const FName Unconscious  = TEXT("Unconscious");  // 昏迷
    static const FName Dejection    = TEXT("Dejection");    // 沮喪
    static const FName Decadence    = TEXT("Decadence");    // 頹廢
    static const FName Rage         = TEXT("Rage");         // 憤怒
    static const FName Madness      = TEXT("Madness");      // 瘋狂
    static const FName Illusion     = TEXT("Illusion");     // 幻象
    static const FName Weakness     = TEXT("Weakness");     // 虛弱
    static const FName Aging        = TEXT("Aging");        // 老化
    static const FName Curse        = TEXT("Curse");        // 詛咒
    static const FName Tracked      = TEXT("Tracked");      // 被追蹤
    static const FName CritWound    = TEXT("CritWound");    // 重創
    static const FName Excitement   = TEXT("Excitement");   // 興奮
}

// ── 週期傷害基底（內部用）────────────────────────────────────────────────────
// 每層獨立計時，0.5s 觸發一次傷害；bUseDirect=true 為真實傷害（流血）。
// bLethal=false 時在瀕死（HP<=MaxHP*5%）停止輸出。
class FPeriodicDamageStatus : public FAbnormalStatusEffect
{
protected:
    static constexpr float NearDeathRatio = 0.05f;

    float             DmgPerTick;
    ESkillElementType DmgElem;
    float             TickInterval;
    bool              bUseDirect;
    bool              bLethal;
    int32             MaxStacksVal;
    float             TickTimer = 0.f;

    FPeriodicDamageStatus(float Dmg, ESkillElementType Elem, float Duration,
                          int32 MaxStacks, float Interval = 0.5f,
                          bool bDirect = false, bool bIsLethal = false)
        : DmgPerTick(Dmg), DmgElem(Elem), TickInterval(Interval)
        , bUseDirect(bDirect), bLethal(bIsLethal), MaxStacksVal(MaxStacks)
    { RemainingDuration = Duration; }

public:
    bool  IsStackable()  const override { return true; }
    int32 GetMaxStacks() const override { return MaxStacksVal; }

    void OnProcess(float DeltaTime, IElementalTarget* Target) override
    {
        if (!Target) return;
        TickTimer += DeltaTime;
        while (TickTimer >= TickInterval)
        {
            TickTimer -= TickInterval;
            if (!bLethal && Target->GetHp() <= Target->GetMaxHp() * NearDeathRatio) break;
            if (bUseDirect)
                Target->TakeDirectDamage(DmgPerTick);
            else
                Target->TakeElementalDamage(DmgPerTick, DmgElem);
        }
    }
};

// ─── 負面狀態 ─────────────────────────────────────────────────────────────────

// 燒傷（可疊 10，3s，每層每 0.5s 10 火元素，瀕死停止）
class FBurnStatus final : public FPeriodicDamageStatus
{
public:
    explicit FBurnStatus(float Duration = 3.f, float Dmg = 10.f, int32 MaxS = 10)
        : FPeriodicDamageStatus(Dmg, ESkillElementType::Fire, Duration, MaxS) {}
    FName GetStatusId()    const override { return AbnormalStatusId::Burn; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("燒傷")); }
};

// 窒息（不可疊，每 0.5s 扣 MaxHP*15%，致命，由氧氣系統外部移除）
class FSuffocationStatus final : public FAbnormalStatusEffect
{
    float TickTimer    = 0.f;
    float TickInterval;
    float DmgRatio;
public:
    explicit FSuffocationStatus(float Interval = 0.5f, float Ratio = 0.15f)
        : TickInterval(Interval), DmgRatio(Ratio)
    { RemainingDuration = 1e30f; }

    FName GetStatusId()    const override { return AbnormalStatusId::Suffocation; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("窒息")); }

    void OnProcess(float DeltaTime, IElementalTarget* Target) override
    {
        if (!Target) return;
        TickTimer += DeltaTime;
        while (TickTimer >= TickInterval)
        {
            TickTimer -= TickInterval;
            Target->TakeDirectDamage(Target->GetMaxHp() * DmgRatio);
        }
    }
};

// 潮濕（不可疊，5s，OnApply 賦予水元素 Aura；impl 在 .cpp）
class FWetStatus final : public FAbnormalStatusEffect
{
public:
    explicit FWetStatus(float Duration = 5.f) { RemainingDuration = Duration; }
    FName GetStatusId()    const override { return AbnormalStatusId::Wet; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("潮濕")); }
    void  OnApply(IElementalTarget* Target) override;
};

// 凍傷（可疊 5，3s，每層每 0.5s 5 冰元素，瀕死停止；疊滿觸發結凍由 Component 處理）
class FFrostbiteStatus final : public FPeriodicDamageStatus
{
public:
    explicit FFrostbiteStatus(float Duration = 3.f, float Dmg = 5.f, int32 MaxS = 5)
        : FPeriodicDamageStatus(Dmg, ESkillElementType::Ice, Duration, MaxS) {}
    FName GetStatusId()    const override { return AbnormalStatusId::Frostbite; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("凍傷")); }
};

// 結凍（不可疊，行動封鎖，物理防禦完全移除）
class FFrozenStatus final : public FAbnormalStatusEffect
{
public:
    explicit FFrozenStatus(float Duration = 2.f) { RemainingDuration = Duration; }
    FName GetStatusId()    const override { return AbnormalStatusId::Frozen; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("結凍")); }
    bool  GetImmobilizes()      const override { return true; }
    bool  GetCannotCastSkills() const override { return true; }
    float GetDefensePenalty()   const override { return 1.f; }
};

// 中毒（可疊 99999，3s，每層每 0.5s 8 毒元素，瀕死停止）
class FPoisonStatus final : public FPeriodicDamageStatus
{
public:
    explicit FPoisonStatus(float Duration = 3.f, float Dmg = 8.f, int32 MaxS = 99999)
        : FPeriodicDamageStatus(Dmg, ESkillElementType::Poison, Duration, MaxS) {}
    FName GetStatusId()    const override { return AbnormalStatusId::Poison; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("中毒")); }
};

// 暗蝕（可疊 10，3s，每層每 0.5s 20 暗元素，致命不停止）
class FDarkErosionStatus final : public FPeriodicDamageStatus
{
public:
    explicit FDarkErosionStatus(float Duration = 3.f, float Dmg = 20.f, int32 MaxS = 10)
        : FPeriodicDamageStatus(Dmg, ESkillElementType::Dark, Duration, MaxS, 0.5f, false, true) {}
    FName GetStatusId()    const override { return AbnormalStatusId::DarkErosion; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("暗蝕")); }
};

// 即死（OnApply 直接致命）
class FInstantDeathStatus final : public FAbnormalStatusEffect
{
public:
    explicit FInstantDeathStatus() { RemainingDuration = 0.f; }
    FName GetStatusId()    const override { return AbnormalStatusId::InstantDeath; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("即死")); }
    void OnApply(IElementalTarget* Target) override
    {
        if (Target) Target->TakeDirectDamage(1e9f);
    }
};

// 麻痺（可疊 10，1s，每層每 0.5s 7 雷元素，瀕死停止，行動+技能封鎖）
class FParalysisStatus final : public FPeriodicDamageStatus
{
public:
    explicit FParalysisStatus(float Duration = 1.f, float Dmg = 7.f, int32 MaxS = 10)
        : FPeriodicDamageStatus(Dmg, ESkillElementType::Thunder, Duration, MaxS) {}
    FName GetStatusId()    const override { return AbnormalStatusId::Paralysis; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("麻痺")); }
    bool  GetImmobilizes()      const override { return true; }
    bool  GetCannotCastSkills() const override { return true; }
};

// 能量封印（不可疊，持續時間由外部決定，鎖住所有 MP）
class FEnergySeaStatus final : public FAbnormalStatusEffect
{
public:
    explicit FEnergySeaStatus(float Duration = 5.f) { RemainingDuration = Duration; }
    FName GetStatusId()    const override { return AbnormalStatusId::EnergySeal; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("能量封印")); }
    bool  GetCannotUseMP() const override { return true; }
};

// 封技（不可疊，持續時間由外部決定，技能+被動全失效）
class FSkillSealStatus final : public FAbnormalStatusEffect
{
public:
    explicit FSkillSealStatus(float Duration = 5.f) { RemainingDuration = Duration; }
    FName GetStatusId()    const override { return AbnormalStatusId::SkillSeal; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("封技")); }
    bool  GetCannotCastSkills() const override { return true; }
};

// 流血（可疊 10，3s，每層每 0.5s 8 真實傷害，瀕死停止）
class FBleedStatus final : public FPeriodicDamageStatus
{
public:
    explicit FBleedStatus(float Duration = 3.f, float Dmg = 8.f, int32 MaxS = 10)
        : FPeriodicDamageStatus(Dmg, ESkillElementType::None, Duration, MaxS, 0.5f, true) {}
    FName GetStatusId()    const override { return AbnormalStatusId::Bleed; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("流血")); }
};

// 石化（不可疊，行動+技能封鎖，物理防禦完全移除）
class FPetrificationStatus final : public FAbnormalStatusEffect
{
public:
    explicit FPetrificationStatus(float Duration = 5.f) { RemainingDuration = Duration; }
    FName GetStatusId()    const override { return AbnormalStatusId::Petrification; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("石化")); }
    bool  GetImmobilizes()      const override { return true; }
    bool  GetCannotCastSkills() const override { return true; }
    float GetDefensePenalty()   const override { return 1.f; }
};

// 暈眩（不可疊，行動+技能封鎖）
class FDizzyStatus final : public FAbnormalStatusEffect
{
public:
    explicit FDizzyStatus(float Duration = 3.f) { RemainingDuration = Duration; }
    FName GetStatusId()    const override { return AbnormalStatusId::Dizzy; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("暈眩")); }
    bool  GetImmobilizes()      const override { return true; }
    bool  GetCannotCastSkills() const override { return true; }
};

// 癱瘓（不可疊，行動封鎖但技能可用）
class FCrippleStatus final : public FAbnormalStatusEffect
{
public:
    explicit FCrippleStatus(float Duration = 5.f) { RemainingDuration = Duration; }
    FName GetStatusId()    const override { return AbnormalStatusId::Cripple; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("癱瘓")); }
    bool  GetImmobilizes() const override { return true; }
    // GetCannotCastSkills 保持 false = 技能仍可使用
};

// 恐懼（可疊 10，3s，每層 -10% 攻擊/移速/攻速；疊滿觸發極度恐懼由 Component 處理）
class FFearStatus final : public FAbnormalStatusEffect
{
    int32 MaxStacksVal;
public:
    explicit FFearStatus(float Duration = 3.f, int32 MaxS = 10)
        : MaxStacksVal(MaxS) { RemainingDuration = Duration; }
    FName GetStatusId()    const override { return AbnormalStatusId::Fear; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("恐懼")); }
    bool  IsStackable()    const override { return true; }
    int32 GetMaxStacks()   const override { return MaxStacksVal; }
    float GetAttackPenalty()      const override { return 0.1f; }
    float GetSpeedPenalty()       const override { return 0.1f; }
    float GetAttackSpeedPenalty() const override { return 0.1f; }
};

// 極度恐懼（不可疊，恐懼疊滿時由 Component 觸發，恐懼降層時由 Component 移除）
class FExtremeFearStatus final : public FAbnormalStatusEffect
{
public:
    explicit FExtremeFearStatus() { RemainingDuration = 1e30f; }
    FName GetStatusId()    const override { return AbnormalStatusId::ExtremeFear; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("極度恐懼")); }
    bool  GetImmobilizes()      const override { return true; }
    bool  GetCannotCastSkills() const override { return true; }
};

// 噩運（可疊 10，3s，每層 -5 幸運值）
class FBadLuckStatus final : public FAbnormalStatusEffect
{
    int32 MaxStacksVal;
    float LuckPenPerStack;
public:
    explicit FBadLuckStatus(float Duration = 3.f, int32 MaxS = 10, float PenPerStack = 5.f)
        : MaxStacksVal(MaxS), LuckPenPerStack(PenPerStack) { RemainingDuration = Duration; }
    FName GetStatusId()    const override { return AbnormalStatusId::BadLuck; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("噩運")); }
    bool  IsStackable()    const override { return true; }
    int32 GetMaxStacks()   const override { return MaxStacksVal; }
    float GetLuckPenalty() const override { return LuckPenPerStack; }
};

// 採掘疲勞（可疊 3，180s，每層 -30% 採掘速度）
class FMiningFatigueStatus final : public FAbnormalStatusEffect
{
    int32 MaxStacksVal;
    float SpeedPenPerStack;
public:
    explicit FMiningFatigueStatus(float Duration = 180.f, int32 MaxS = 3, float PenPerStack = 0.3f)
        : MaxStacksVal(MaxS), SpeedPenPerStack(PenPerStack) { RemainingDuration = Duration; }
    FName GetStatusId()           const override { return AbnormalStatusId::MiningFatigue; }
    FText GetDisplayName()        const override { return FText::FromString(TEXT("採掘疲勞")); }
    bool  IsStackable()           const override { return true; }
    int32 GetMaxStacks()          const override { return MaxStacksVal; }
    float GetMiningSpeedPenalty() const override { return SpeedPenPerStack; }
};

// ─── 正面狀態 ─────────────────────────────────────────────────────────────────

// 霸體（免疫所有行動/技能封鎖）
class FSuperarmorStatus final : public FAbnormalStatusEffect
{
public:
    explicit FSuperarmorStatus(float Duration = 1.f) { RemainingDuration = Duration; }
    FName GetStatusId()    const override { return AbnormalStatusId::SuperArmor; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("霸體")); }
    EAbnormalPolarity GetPolarity()  const override { return EAbnormalPolarity::Positive; }
    bool  GetHasSuperArmor()        const override { return true; }
};

// 虛化（穿透；碰撞層切換待 Phase C）
class FEtherealStatus final : public FAbnormalStatusEffect
{
public:
    explicit FEtherealStatus(float Duration = 1.f) { RemainingDuration = Duration; }
    FName GetStatusId()    const override { return AbnormalStatusId::Ethereal; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("虛化")); }
    EAbnormalPolarity GetPolarity() const override { return EAbnormalPolarity::Positive; }
    bool  GetIsEthereal()           const override { return true; }
};

// 無敵（傷害歸零；FCombatResolver 接入待 Phase C）
class FInvincibleStatus final : public FAbnormalStatusEffect
{
public:
    explicit FInvincibleStatus(float Duration = 1.f) { RemainingDuration = Duration; }
    FName GetStatusId()    const override { return AbnormalStatusId::Invincible; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("無敵")); }
    EAbnormalPolarity GetPolarity() const override { return EAbnormalPolarity::Positive; }
    bool  GetIsInvincible()         const override { return true; }
};

// 基礎元素抵抗
class FBasicElemResistStatus final : public FAbnormalStatusEffect
{
public:
    explicit FBasicElemResistStatus(float Duration = 5.f) { RemainingDuration = Duration; }
    FName GetStatusId()    const override { return AbnormalStatusId::BasicElemResist; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("基礎元素抵抗")); }
    EAbnormalPolarity GetPolarity()          const override { return EAbnormalPolarity::Positive; }
    bool  GetHasBasicElemResistance()        const override { return true; }
};

// 進階元素抵抗
class FAdvancedElemResistStatus final : public FAbnormalStatusEffect
{
public:
    explicit FAdvancedElemResistStatus(float Duration = 5.f) { RemainingDuration = Duration; }
    FName GetStatusId()    const override { return AbnormalStatusId::AdvElemResist; }
    FText GetDisplayName() const override { return FText::FromString(TEXT("進階元素抵抗")); }
    EAbnormalPolarity GetPolarity()             const override { return EAbnormalPolarity::Positive; }
    bool  GetHasAdvancedElemResistance()        const override { return true; }
};
