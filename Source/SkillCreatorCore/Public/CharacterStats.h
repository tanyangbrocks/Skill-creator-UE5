#pragma once
#include "CoreMinimal.h"
#include "ElementType.h"
#include "CharacterStats.generated.h"

// 玩家角色完整數值結構（對應 Godot CharacterStats.cs W-5a）。
// ⚠️ 元素親和力 / 輸出 / 抗性 TMap → 元素系統（M-5+）建立後補入。
USTRUCT(BlueprintType)
struct FCharacterStats
{
    GENERATED_BODY()

    // ── 生命 ──────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HP")
    float MaxHpBase   = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HP")
    float HpRegenRate = 0.f;

    // ── 防禦 ──────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense")
    float BaseDefense     = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense")
    float DamageReduction = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense")
    float AntiCombo       = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense")
    float AntiCrit        = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense")
    float Immunity        = 0.f;

    // ── 攻擊 ──────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    float Power            = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    float PhysicalDmgPct   = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    float CritRate         = 0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    float CritDmgMult      = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    float SuperCritRate    = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    float SuperCritDmgMult = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    float Thorns           = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    float Lifesteal        = 0.f;

    // ── MP ────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MP")
    float MaxMpBase    = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MP")
    float MpRegenRate  = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MP")
    float MpOutput     = 1.f;   // MP 輸出效率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MP")
    float MpEfficiency = 1.f;   // MP 功率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MP")
    float MpControl    = 1.f;   // MP 掌控度

    // ── 機動 ──────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mobility")
    float MoveSpeedMult = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mobility")
    float AtkSpeedMult  = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mobility")
    float DodgeRate     = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mobility")
    float HitRate       = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mobility")
    float DoubleHitRate = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mobility")
    float TripleHitRate = 0.f;

    // ── 社會 / 外貌（stub，無 NPC 系統時不生效）──────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Social")
    float Appearance   = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Social")
    float Temperament  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Social")
    float TrustLevel   = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Social")
    float AffinityScore = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Social")
    float Luck         = 0.f;

    // ── 生產 / 探索（stub）──────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Exploration")
    float Deliciousness  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Exploration")
    float Fragrance      = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Exploration")
    float MaterialRarity = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Exploration")
    float Insight        = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Exploration")
    float Stealth        = 0.f;

    // ── 元素親和力 / 輸出 / 抗性（對應 Godot CharacterStats.cs）────
    // ElemAffinity：受到元素 Aura 時的額外加成（0 = 無加成）
    // ElemOutputMult：施放元素技能傷害倍率（1.0 = 標準）
    // ElemResistance：受到元素傷害的抵抗係數（0 = 無抗性；1.0 = 完全免疫）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Element")
    TMap<ESkillElementType, float> ElemAffinity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Element")
    TMap<ESkillElementType, float> ElemOutputMult;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Element")
    TMap<ESkillElementType, float> ElemResistance;

    float GetElemAffinity(ESkillElementType E)   const
    { const float* V = ElemAffinity.Find(E);   return V ? *V : 0.f; }
    float GetElemOutputMult(ESkillElementType E) const
    { const float* V = ElemOutputMult.Find(E); return V ? *V : 1.f; }
    float GetElemResistance(ESkillElementType E) const
    { const float* V = ElemResistance.Find(E); return V ? *V : 0.f; }

    void SetElemAffinity(ESkillElementType E, float V)   { ElemAffinity[E]   = V; }
    void SetElemOutputMult(ESkillElementType E, float V) { ElemOutputMult[E] = V; }
    void SetElemResistance(ESkillElementType E, float V) { ElemResistance[E] = V; }

    // ── 經驗加成（stub）──────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Experience")
    float SkillExpBonus      = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Experience")
    float LevelExpBonus      = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Experience")
    float NonCombatExpBonus  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Experience")
    float DispatchEfficiency = 1.f;

    // ── 天賦點（W-10，初始皆 50）──────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Talent")
    int32 TalentConstitution = 50;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Talent")
    int32 TalentStrength     = 50;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Talent")
    int32 TalentEndurance    = 50;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Talent")
    int32 TalentAgility      = 50;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Talent")
    int32 TalentWisdom       = 50;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Talent")
    int32 TalentCharisma     = 50;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Talent")
    int32 TalentLuck         = 50;

    // ── 能力點（W-10 分配，初始 0）──────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    int32 ConstitutionPoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    int32 StrengthPoints     = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    int32 EndurancePoints    = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    int32 AgilityPoints      = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    int32 WisdomPoints       = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    int32 CharismaPoints     = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    int32 LuckPoints         = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Other")
    float BloodlineStrength = 1.f;
};
