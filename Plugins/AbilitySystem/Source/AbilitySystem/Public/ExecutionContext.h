#pragma once
#include "CoreMinimal.h"
#include "Instruction.h"
#include "GridPos.h"

// ── 執行狀態機 ────────────────────────────────────────────────────
enum class EExecutionState : uint8
{
    Running,
    Waiting,            // Wait 計時中；PC 停在原位，Step 計時歸零後 PC++
    WaitingFrames,      // SleepFrames 計幀中；每幀遞減，歸零後 PC++
    WaitingSignal,      // OnReceive 等待廣播訊號
    WaitingCondition,   // DetectHp/Mp/Hit/EntityEnter 條件等待
    WaitingRisingEdge,
    WaitingFallingEdge,
    Completed,
    Fizzled,            // 目標消失或 While 超限（MP 不退還）
};

// ── 實體快照（純 POD，可在背景執行緒讀取）────────────────────────
struct FEntityInfo
{
    int32    Id      = -1;
    FGridPos Position;
    float    Hp      = 0.f;
    float    MaxHp   = 0.f;
};

// ── ForEach 迭代器堆疊元素 ────────────────────────────────────────
struct FEntityIterFrame
{
    TArray<FEntityInfo> Entities;
    int32               CurrentIndex = 0;
};

// ── Raycast 結果 ──────────────────────────────────────────────────
struct FRaycastResult
{
    FGridPos HitPos;
    int32    MatId = 0;
    bool     bHit  = false;
};

// ══════════════════════════════════════════════════════════════════
//  FExecutionContext — 單一技能施放的完整執行狀態
//
//  純 C++ 類別（非 UObject）；所有遊戲層依賴以 TFunction delegate 注入。
//  靜態成員（GlobalVars 等）為全域狀態，定義在 ExecutionContext.cpp。
// ══════════════════════════════════════════════════════════════════
class ABILITYSYSTEM_API FExecutionContext
{
public:
    // ── 編譯後的指令序列 ──────────────────────────────────────────
    TArray<FInstruction> Code;
    int32                PC    = 0;
    EExecutionState      State = EExecutionState::Running;

    float WaitRemaining       = 0.f;
    int32 WaitFramesRemaining = 0;
    float MpConsumed          = 0.f;
    int32 LoopcastIndex       = 0;

    // ── 迴圈計數器 ────────────────────────────────────────────────
    TArray<int32>      LoopCounters;        // RepeatN 堆疊
    TMap<int32, int32> WhileIterCounters;   // WhileCheck 安全計數：PC → 已迭代次數

    // ── 遊戲層 delegate（由呼叫方在 Submit 前注入）────────────────
    TFunction<TArray<FEntityInfo>(float Radius)>                        EntityQuery;
    TFunction<FRaycastResult(FGridPos Start, FVector Dir, float Dist)>  RaycastQuery;
    TFunction<FGridPos()>                                               FocalPointQuery;
    TFunction<float(FName Key)>                                         PlayerStatsQuery;
    TFunction<bool(FName Signal)>                                       HasSignalFn;
    TFunction<void(FName Signal)>                                       BroadcastFn;
    TFunction<float(FName StatKey)>                                     BattleStatFn;
    TFunction<bool()>                                                   TookDamageThisTickFn;
    TFunction<void(int32 Radius)>                                       AnchorAction;
    TFunction<void()>                                                   RollbackAction;
    TFunction<void(int32 Mode)>                                         SetActivationModeFn;
    TFunction<void(FName FilterType, FName Mode, bool bOneShot, float Threshold, float CapValue)>
                                                                        RegisterFilterFn;
    // InvokeTotem 分派（TryCast 時按技能整構設定，捕捉 slot 對照表）
    TFunction<void(FExecutionContext&, FName TotemName)>                InvokeTotemFn;
    // GAS-4: EntityId==-1 -> caster; >=0 -> ForEach current entity
    TFunction<void(FName StatusTagLeaf, float Level, int32 EntityId)>  ApplyGasEffectFn;

    // ── ForEach 實體迭代 ──────────────────────────────────────────
    TArray<FEntityIterFrame> EntityIterators;
    TOptional<FEntityInfo>   CurrentIterEntity;

    // ── 待處理遊戲動作（SpellRunner 消費後清除）──────────────────
    int32    PendingEntityDamageId     = -1;
    float    PendingEntityDamageAmount = 0.f;
    int32    PendingEntityMoveId       = -1;
    FGridPos PendingEntityMovePos;

    // ── 等待狀態 ──────────────────────────────────────────────────
    FName WaitingSignalName;
    FName WaitingConditionKey;
    float WaitingConditionThreshold = 0.f;

    // ── 邊緣觸發狀態 ──────────────────────────────────────────────
    int32             WaitingEdgePC = -1;
    TMap<int32, bool> EdgeState;
    TSet<int32>       PulseArmed;

    // ── 變數（instance 獨立；Global 跨技能整構共享）─────────────
    TMap<FName, float>         InstanceVars;
    TMap<FName, TArray<float>> InstanceLists;

    static TMap<FName, float>         GlobalVars;
    static TMap<FName, TArray<float>> GlobalLists;
    static TMap<FName, float>         TaskCounters;
    static TSet<FName>                TaskCounterReached;
    static bool                       bTraceMode;

    // ── 技能因子執行結果 ──────────────────────────────────────────
    TSet<FName> HitTotems;
    TSet<FName> DoneTotems;
    TSet<FName> FizzledTotems;

    // ── 待執行的 Invoke（SpellRunner 在 Step 後消費）─────────────
    FName PendingInvokeSpell;
    FName PendingInvokeTotem;

    // ── 其他執行期狀態 ────────────────────────────────────────────
    TMap<int32, int32> AlternateCounts;
    TOptional<FGridPos> FixedOrigin;

    // ── 生命週期 ──────────────────────────────────────────────────
    FExecutionContext() = default;
    explicit FExecutionContext(TArray<FInstruction> InCode);

    bool IsFinished() const
    {
        return State == EExecutionState::Completed || State == EExecutionState::Fizzled;
    }

    // ── List 輔助 ─────────────────────────────────────────────────
    TArray<float>&  GetOrCreateList(FName Name, bool bGlobal);
    TArray<float>*  GetList(FName Name, bool bGlobal);
};
