#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UElementalAuraComponent.generated.h"

// 單一元素 Aura 條目（M-5 元素系統建立後填充反應邏輯）
USTRUCT()
struct FAuraEntry
{
    GENERATED_BODY()
    UPROPERTY() uint8 ElementId = 0;
    UPROPERTY() float Duration  = 0.f;
};

// 管理生物身上的多重元素 Aura，輸出聚合屬性給 Enemy / Character 讀取。
// M-4：只有聚合輸出欄位（永遠為 0）；M-5 元素系統建立後填入 Process() 邏輯。
UCLASS(ClassGroup=SkillCreator, meta=(BlueprintSpawnableComponent))
class SKILLCREATORRUNTIME_API UElementalAuraComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UElementalAuraComponent();

    // ── 聚合輸出（每幀由 Process() 重算；M-4 皆為 0）────────────
    // 移速懲罰加總（0 = 正常，0.3 = 移動計時器延長 30%）
    float SpeedPenalty        = 0.f;
    // 是否完全無法移動（結凍 / 感電麻痺）
    bool  bIsImmobilized      = false;
    // 受傷倍率加成（0 = 正常，0.2 = 多受 20%）
    float DamageTakenBonus    = 0.f;
    // 防禦力懲罰（0 = 正常，0.1 = 防禦降 10%）
    float DefensePenalty      = 0.f;
    // 環境溫度偏移（°C），供 CharacterState 體溫計算讀取
    float AuraTemperatureShift = 0.f;

    // 每幀呼叫，更新 Aura 倒計時並重算聚合輸出
    void Process(float DeltaTime);

    // 清除所有 Aura 及效果（死亡 / 復活用）
    void Reset();

    // TODO M-5: Apply(ElementId, Duration, IElementalTarget*) — 帶冷卻應用
    //           ApplyImmediate(ElementId, Duration, IElementalTarget*) — 無冷卻（技能命中）
    //           ElementalReactionTable 觸發反應

private:
    UPROPERTY() TArray<FAuraEntry> Auras;

    void RecalcAggregates();
};
