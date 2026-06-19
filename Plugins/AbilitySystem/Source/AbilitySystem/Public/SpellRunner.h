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
    // MpCost：此次施放已扣除的 MP，供 PruneAfter 退還用（對應 Godot ActiveSpell.MpCost）
    void Submit(TUniquePtr<FExecutionContext> Ctx, float MpCost = 0.f);

    // ── 每幀驅動（從 AActor::Tick 或 UActorComponent::TickComponent 呼叫）
    void Tick(float DeltaTime);

    // ── 清除所有執行中的技能整構
    void PruneAll();

    // ── 清除已執行超過 MaxAgeSeconds 的技能整構（對應 Godot SpellRunner.PruneAfter）
    void PruneAfter(float MaxAgeSeconds);

    int32 GetActiveCount() const { return ActiveContexts.Num(); }

    // ── 遊戲層 callback（SpellRunner 在每次 InvokeTotem 等之後呼叫）──
    // C-5：PruneAfter 清除時呼叫，通知遊戲層退還 MP（對應 Godot SpellRunner.cs:118）
    TFunction<void(float MpToRefund)>                        OnMpRefund;
    TFunction<void(FExecutionContext&, FName TotemName)>     OnInvokeTotem;
    TFunction<void(int32 EntityId, float Damage)>            OnEntityDamage;
    TFunction<void(int32 EntityId, FGridPos NewPos)>         OnEntityMove;
    // 連段：遊戲層負責建立並回傳已注入 delegate 的新 Context；nullptr 表示拒絕連段
    TFunction<TUniquePtr<FExecutionContext>(FName SpellName)> OnBuildComboContext;

private:
    struct FActiveEntry
    {
        TUniquePtr<FExecutionContext> Ctx;
        int32                        ComboDepth  = 0;
        float                        ElapsedTime = 0.f;
        float                        MpCost      = 0.f;  // C-5：施放時扣除的 MP，PruneAfter 退還用
    };

    TArray<FActiveEntry>       ActiveContexts;
    TUniquePtr<FExecutionLoop> Loop;

    void Advance(FActiveEntry& Entry, float DeltaTime);
};
