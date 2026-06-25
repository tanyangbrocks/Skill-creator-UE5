#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ElementType.h"
#include "ElementalStatusEffect.h"
#include "SnapshotTypes.h"
#include "UElementalAuraComponent.generated.h"

// 生物身上的單一元素 Aura 條目（原始元素，非反應效果）
USTRUCT()
struct FAuraEntry
{
    GENERATED_BODY()
    UPROPERTY() ESkillElementType Element = ESkillElementType::None;
    UPROPERTY() float        Duration = 0.f;
};

// 管理生物身上的多重元素 Aura 與元素狀態效果（對應 Godot ElementalAuraComponent.cs）
// Enemy 和 PlayerCharacter 各持有一份。
UCLASS(ClassGroup=SkillCreator, meta=(BlueprintSpawnableComponent))
class SKILLCREATORRUNTIME_API UElementalAuraComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UElementalAuraComponent();

    // ── 應用冷卻常數（環境接觸用）────────────────────────────────────
    static constexpr float ApplicationCooldownSec = 1.0f;
    static constexpr float DefaultAuraDuration    = 5.0f;

    // ── 溫度偏移常數（W-5b 體溫系統連接）────────────────────────────
    static constexpr float FireAuraTempShift  = +15.f;
    static constexpr float IceAuraTempShift   = -20.f;
    static constexpr float WaterAuraTempShift =  -8.f;
    static constexpr float AuraTempShiftMax   =  50.f;

    // ── 聚合輸出（每幀由 Process() 重算；供 Enemy / Character 讀取）──
    float SpeedPenalty         = 0.f;  // 移速懲罰加總（0 = 正常）
    bool  bIsImmobilized       = false; // 完全無法移動（結凍 / 感電）
    float DamageTakenBonus     = 0.f;  // 受傷倍率加成
    float DefensePenalty       = 0.f;  // 防禦力懲罰（0–1）
    float AuraTemperatureShift = 0.f;  // 環境溫度偏移（°C）

    // ── 固有元素（生物種類 / 裝備 / 天生屬性）────────────────────────
    // 永久存在，不隨反應被消耗；incoming element 仍會與它配對觸發反應，
    // 但 NativeElement 本身留存，可對下一個 incoming 再次觸發。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Elemental")
    ESkillElementType NativeElement = ESkillElementType::None;

    // ── 主要 API ──────────────────────────────────────────────────────
    // 帶冷卻（環境接觸：踩材質格 / 停留區域）；同元素 1 秒內只觸發一次
    bool Apply(ESkillElementType Element, float Duration, IElementalTarget* Target);
    // 無冷卻（技能命中 / CA 碰撞）
    void ApplyImmediate(ESkillElementType Element, float Duration, IElementalTarget* Target);
    // 直接套用凍結（刻印用）；跳過元素反應機制
    void ApplyFreeze(float Duration, IElementalTarget* Target);
    // 直接套用蔓生緩速（刻印用）；跳過元素反應機制
    void ApplySlow(float Duration, IElementalTarget* Target);

    // 每幀呼叫：更新冷卻、Aura 計時、效果 tick，並重算聚合輸出
    void Process(float DeltaTime);
    // 清除所有 Aura 與效果（死亡 / 復活用）
    void Reset();

    // ── 快照 API（對應 Godot AuraSnapshot.cs）─────────────────────────────
    FAuraSnapshot TakeAuraSnapshot() const;
    void          RestoreAuraSnapshot(const FAuraSnapshot& Snap);
    static TUniquePtr<FElementalStatusEffect> CreateEffect(const FAuraEffectData& Data);

private:
    UPROPERTY() TArray<FAuraEntry> Auras;

    // 元素狀態效果（反應結果）；不走 UPROPERTY，runtime-only
    TArray<TUniquePtr<FElementalStatusEffect>> ActiveEffects;
    // 環境接觸冷卻剩餘秒數
    TMap<ESkillElementType, float> CdLeft;

    void ApplyInternal(ESkillElementType Element, float Duration, IElementalTarget* Target);
    void AddEffect(TUniquePtr<FElementalStatusEffect> Fx, IElementalTarget* Target);
    void RecalcAggregates();
};
