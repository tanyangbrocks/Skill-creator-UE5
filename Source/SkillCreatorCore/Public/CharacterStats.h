#pragma once
#include "CoreMinimal.h"
#include "ElementType.h"
#include "CharacterStats.generated.h"

// 玩家角色完整數值結構（對應 Godot CharacterStats.cs W-5a + B 系列補齊）。
USTRUCT(BlueprintType)
struct FCharacterStats
{
    GENERATED_BODY()

    // ── 生命 ──────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HP")
    float MaxHpBase    = 100.f;
    // HP/秒（Godot CharacterStats.cs:18 HP/秒；B-4 計時器每 0.5s 扣 0.5 倍）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HP")
    float HpRegenRate  = 0.f;

    // ── 物理防禦 ─────────────────────────────────────────────────
    // 受物理攻擊：step1 = max(0, PhysAtk - PhysicalDefense)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense|Physical")
    float PhysicalDefense         = 0.f;
    // step2 = max(0, step1 - PhysicalDamageReduction)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense|Physical")
    float PhysicalDamageReduction = 0.f;

    // ── 能量防禦 ─────────────────────────────────────────────────
    // 受能量攻擊通用防禦（次於特定 MP 防禦）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense|Energy")
    float EnergyDefense           = 0.f;
    // 能量傷害通用減傷（次於特定 MP 減傷）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense|Energy")
    float EnergyDamageReduction   = 0.f;

    // ── 特定 MP 防禦 / 減傷（stub，key = ManaTypeKey）─────────────
    // 能量傷害 4 步：特定MP防禦 → 通用能量防禦 → 特定MP減傷 → 通用能量減傷
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense|MP")
    TMap<FName, float> MpSpecificDefense;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense|MP")
    TMap<FName, float> MpSpecificDamageReduction;

    float GetMpDefense(FName Key)           const { const float* V = MpSpecificDefense.Find(Key);           return V ? *V : 0.f; }
    float GetMpDamageReduction(FName Key)   const { const float* V = MpSpecificDamageReduction.Find(Key);   return V ? *V : 0.f; }

    // ── 其它抗性 ─────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense")
    float AntiCombo              = 0.f;  // 抗連擊率（抵銷攻擊方的二/三連擊率）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense")
    float AntiCrit               = 0.f;  // 抗暴率（抵銷攻擊方的暴擊率）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense")
    float AntiCritDmgReduction   = 0.f;  // 抗暴傷（承受暴擊時，抵銷攻擊方暴擊傷倍率）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense")
    float AntiSuperCritRate      = 0.f;  // 抗超暴率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense")
    float AntiSuperCritDmgReduction = 0.f; // 抗超暴傷
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense")
    float Immunity               = 0.f;

    // ── 基本攻擊 ─────────────────────────────────────────────────
    // Power：物理攻擊力（保留原語義，用於武器/技能計算）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    float Power          = 0.f;
    // Strength：計算後的最終力量值（衍生自 StrengthPoints + TalentStrength，B-1 只開欄位）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    float Strength       = 0.f;
    // EnergyAtk：能量攻擊基礎值（帶 MP 類型的攻擊使用）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    float EnergyAtk      = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    float PhysicalDmgPct = 0.f;

    // ── 暴擊 ─────────────────────────────────────────────────────
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
    // MP/秒（Godot ManaSlot.cs:11 DefaultRegenRate = 1f，每秒回復量）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MP")
    float MpRegenRate  = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MP")
    float MpOutput     = 1.f;   // MP 輸出效率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MP")
    float MpEfficiency = 1.f;   // MP 功率（stub，暫不實作效果）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MP")
    float MpControl    = 1.f;   // MP 掌控度（stub，預設 100%，暫不實作效果）

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
    float Appearance    = 0.f;  // 顏值
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Social")
    float Temperament   = 0.f;  // 氣質
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Social")
    float TrustLevel    = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Social")
    float AffinityScore = 0.f;  // 向後相容舊代碼；好感度用 Affection
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Social")
    float Affection     = 0.f;  // 好感度
    // 各類緣份點（key = 緣份類型名）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Social")
    TMap<FName, float> BondPoints;

    // ── 引敵（stub）─────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Threat")
    float Luck           = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Threat")
    float Deliciousness  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Threat")
    float Fragrance      = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Threat")
    float MaterialRarity = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Threat")
    int32 WantedLevel    = 0;   // 通緝等級（0 = 無；暫不實作）

    // ── 資訊攻防（stub）──────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Exploration")
    float Insight  = 0.f;  // 洞察度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Exploration")
    float Stealth  = 0.f;  // 隱密度

    // ── 元素親和力 / 輸出 / 抗性（對應 Godot CharacterStats.cs）────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Element")
    TMap<ESkillElementType, float> ElemAffinity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Element")
    TMap<ESkillElementType, float> ElemOutputMult;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Element")
    TMap<ESkillElementType, float> ElemResistance;

    float GetElemAffinity(ESkillElementType E)   const { const float* V = ElemAffinity.Find(E);   return V ? *V : 0.f; }
    float GetElemOutputMult(ESkillElementType E) const { const float* V = ElemOutputMult.Find(E); return V ? *V : 1.f; }
    float GetElemResistance(ESkillElementType E) const { const float* V = ElemResistance.Find(E); return V ? *V : 0.f; }

    void SetElemAffinity(ESkillElementType E, float V)   { ElemAffinity[E]   = V; }
    void SetElemOutputMult(ESkillElementType E, float V) { ElemOutputMult[E] = V; }
    void SetElemResistance(ESkillElementType E, float V) { ElemResistance[E] = V; }

    // ── 法則三項（stub，key = 法則名稱）──────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Law")
    TMap<FName, float> LawAffinity;    // 各法則親和力
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Law")
    TMap<FName, float> LawOutputMult;  // 各法則輸出力
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|Law")
    TMap<FName, float> LawResistance;  // 各法則抗性

    // ── 能量三項（stub，key = ManaTypeKey）────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|MpCompatibility")
    TMap<FName, float> MpAffinity;     // 各能量契合度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|MpCompatibility")
    TMap<FName, float> MpOutputMult;   // 各能量輸出力
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats|Stub|MpCompatibility")
    TMap<FName, float> MpResistance;   // 各能量抗性

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

    // ── 基礎能力點（W-10 分配，初始 0）──────────────────────────
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

    // ── 技能/屬性/法則能力點（stub，W-10 三分類）────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    int32 SkillAbilityPoints    = 0;   // 技能能力點
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    int32 ElementAbilityPoints  = 0;   // 屬性效果能力點
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    int32 LawAbilityPoints      = 0;   // 法則效果能力點

    // ── 其它 ─────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Other")
    float BloodlineStrength = 1.f;
};
