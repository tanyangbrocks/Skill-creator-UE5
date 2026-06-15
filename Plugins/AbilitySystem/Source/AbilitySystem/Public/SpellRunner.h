#pragma once
#include "CoreMinimal.h"
#include "ExecutionContext.h"

class FExecutionLoop;

// ══════════════════════════════════════════════════════════════════
//  FSpellRunner — 持久化技能執行容器
//
//  每幀呼叫 Tick()；Submit() 提交一個已注入 delegate 的 Context。
//  遊戲層在 Submit 前把 EntityQuery / PlayerStatsQuery 等注入 Ctx。
//  SpellRunner 本身不持有遊戲層參照，只透過 On* callback 回報事件。
// ══════════════════════════════════════════════════════════════════
class ABILITYSYSTEM_API FSpellRunner
{
public:
    FSpellRunner();
    ~FSpellRunner();

    // ── 提交一個已就緒的技能執行（呼叫方負責注入所有 delegate）─
    void Submit(TUniquePtr<FExecutionContext> Ctx);

    // ── 每幀驅動（從 AActor::Tick 或 UActorComponent::TickComponent 呼叫）
    void Tick(float DeltaTime);

    // ── 清除所有執行中的技能整構
    void PruneAll();

    int32 GetActiveCount() const { return ActiveContexts.Num(); }

    // ── 遊戲層 callback（SpellRunner 在每次 InvokeTotem 等之後呼叫）──
    TFunction<void(FExecutionContext&, FName TotemName)>     OnInvokeTotem;
    TFunction<void(int32 EntityId, float Damage)>            OnEntityDamage;
    TFunction<void(int32 EntityId, FGridPos NewPos)>         OnEntityMove;
    // 連段：遊戲層負責建立並回傳已注入 delegate 的新 Context；nullptr 表示拒絕連段
    TFunction<TUniquePtr<FExecutionContext>(FName SpellName)> OnBuildComboContext;

private:
    struct FActiveEntry
    {
        TUniquePtr<FExecutionContext> Ctx;
        int32                        ComboDepth = 0;
    };

    TArray<FActiveEntry>       ActiveContexts;
    TUniquePtr<FExecutionLoop> Loop;

    void Advance(FActiveEntry& Entry, float DeltaTime);
};
