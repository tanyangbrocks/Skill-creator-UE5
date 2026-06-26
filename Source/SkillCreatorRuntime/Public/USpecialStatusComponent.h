#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpecialStatusTypes.h"
#include "USpecialStatusComponent.generated.h"

class UElementalAuraComponent;
class IElementalTarget;

// ── 特殊狀態管理元件（所有可臨時賦予或移除效果的統一入口）──────────────────
//
// 每個生物掛一個，並列的 UElementalAuraComponent 管元素反應；
// 本元件管「異常狀態」與「其他特殊狀態」，並彙整三類的聚合輸出供角色/UI讀取。
//
// 疊加策略（由 FAbnormalStatusEffect::IsStackable / GetMaxStacks 決定）：
//   IsStackable == false → 同 Id 只維持 1 層，新層刷新 RemainingDuration
//   IsStackable == true  → 每層獨立計時，上限 GetMaxStacks()
//
// 脫戰清除：呼叫 ClearOutOfCombat() 可快速移除帶 bClearOnExitCombat 旗標的效果；
//   由 UCombatStateSubsystem::ExitCombat 在脫戰時呼叫（尚未接線，架構已預留）。
UCLASS(ClassGroup=SkillCreator, meta=(BlueprintSpawnableComponent))
class SKILLCREATORRUNTIME_API USpecialStatusComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USpecialStatusComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ── 聚合輸出（所有類別合算；角色/AI/UI 讀這裡）─────────────────────────
    // 負面
    float TotalSpeedPenalty        = 0.f;   // 移速懲罰加總
    bool  bIsImmobilized           = false; // 無法進行任何基礎動作
    bool  bCannotCastSkills        = false; // 無法施放技能（含被動）
    bool  bCannotUseMP             = false; // 所有 MP 鎖住
    float TotalDamageTakenBonus    = 0.f;   // 受傷倍率加成
    float TotalDefensePenalty      = 0.f;   // 防禦懲罰，已 clamp [0,1]
    float TotalAttackPenalty       = 0.f;   // 攻擊力懲罰加總
    float TotalAttackSpeedPenalty  = 0.f;   // 攻速懲罰加總
    float TotalLuckPenalty         = 0.f;   // 幸運值懲罰加總
    float TotalMiningSpeedPenalty  = 0.f;   // 採掘速度懲罰，已 clamp [0,0.9]
    // 正面
    float TotalAttackBonus         = 0.f;   // 攻擊增益（正面狀態）
    float TotalDefenseBonus        = 0.f;   // 防禦增益（正面狀態）
    bool  bHasSuperArmor           = false; // 霸體：蓋掉所有行動/技能封鎖
    bool  bIsEthereal              = false; // 虛化：穿透
    bool  bIsInvincible            = false; // 無敵：傷害歸零
    bool  bHasBasicElemResistance  = false; // 基礎元素抵抗
    bool  bHasAdvElemResistance    = false; // 進階元素抵抗

    // ── 異常狀態 API ──────────────────────────────────────────────────────
    // 新增一個異常/其他特殊狀態（所有權移交本元件）
    void ApplyStatus(TUniquePtr<FAbnormalStatusEffect> Effect, IElementalTarget* Target = nullptr);
    // 移除指定 Id 的所有層（若 OnRemove 需通知目標，傳入 Target）
    void RemoveStatus(FName StatusId, IElementalTarget* Target = nullptr);
    // 是否有該狀態
    bool  HasStatus(FName StatusId)       const;
    // 目前層數
    int32 GetStackCount(FName StatusId)   const;

    // ── 清除 API ──────────────────────────────────────────────────────────
    void ClearAll(IElementalTarget* Target = nullptr);
    void ClearCategory(ESpecialStatusCategory Category, IElementalTarget* Target = nullptr);
    // 脫戰快速清除（未來由 UCombatStateSubsystem 呼叫）
    void ClearOutOfCombat(IElementalTarget* Target = nullptr);

    // 由 Owner（角色）在 BeginPlay 主動呼叫，供 ProcessEffects 使用（無 RTTI 不可 dynamic_cast）
    void SetOwnerTarget(IElementalTarget* T) { CachedTarget = T; }

private:
    // 元素 Aura（不擁有，BeginPlay 查詢 Owner 上的元件）
    UPROPERTY() TObjectPtr<UElementalAuraComponent> ElementalAura;

    // 異常狀態 + 其他特殊狀態（同一陣列，由 GetCategory() 區分）
    TArray<TUniquePtr<FAbnormalStatusEffect>> ActiveEffects;

    // 由 SetOwnerTarget() 設定，供 ProcessEffects / CheckChainTriggers 使用
    IElementalTarget* CachedTarget = nullptr;
    // 凍傷觸發結凍後的 5 秒免疫計時
    float FrostbiteToFrozenCooldown = 0.f;

    void ProcessEffects(float DeltaTime);
    void RecalcAggregates();
    void CheckChainTriggers();
    int32 GetMaxStacksForStatus(FName StatusId) const;
};
