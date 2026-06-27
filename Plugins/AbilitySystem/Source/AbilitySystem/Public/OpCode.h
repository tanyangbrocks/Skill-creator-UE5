#pragma once
#include "CoreMinimal.h"
#include "OpCode.generated.h"

// VM 位元組碼指令集（對應 Godot OpCode.cs）
// SpellCompiler 將 FBlockNode AST 編譯為 FInstruction 陣列，每條指令帶一個 EOpCode
UENUM(BlueprintType)
enum class EOpCode : uint8
{
    // ── 基本控制流 ────────────────────────────────────────
    Wait,             // 等待 N 秒（SpellRunner 跨幀計時）
    JumpIfFalse,      // 條件為 false 跳到 else/end
    Jump,             // 無條件跳轉（跳過 else 分支）
    SetVar,
    InvokeTotem,
    InvokeSpell,

    // ── 迴圈 ─────────────────────────────────────────────
    RepeatPush,       // RepeatN：推入迭代次數
    RepeatStep,       // RepeatN：遞減計數，>0 跳回；否則結束
    WhileCheck,       // RepeatWhile：條件 false 跳出
    ForEachStart,     // ForEachNearby：查詢實體，空則跳出
    ForEachStep,      // ForEachNearby：前進迭代器

    // ── 比較與查詢 ────────────────────────────────────────
    StoreCompare,
    QueryNearest,
    GetEntityProp,
    StoreEntityProp,

    // ── 列表 ─────────────────────────────────────────────
    ListCreate,
    ListAppend,
    ListPop,
    ListGet,
    ListDequeue,
    ListSet,
    ListLength,
    ListContains,
    ListRemoveAt,
    ListClear,

    // ── 廣播 ─────────────────────────────────────────────
    Broadcast,
    OnReceive,

    // ── 進階控制流 ────────────────────────────────────────
    Die,
    SleepFrames,

    // ── 執行追蹤 ─────────────────────────────────────────
    ReadExecStat,

    // ── 任務計數器 ────────────────────────────────────────
    TaskCounterSet,
    TaskCounterAdd,
    TaskCounterGet,
    TaskCounterOnReach,
    TaskCounterReset,

    // ── 向量運算 ─────────────────────────────────────────
    VecMake,
    VecGetComp,
    VecAdd,
    VecSub,
    VecScale,
    VecNegate,
    VecNorm,
    VecLength,
    VecDot,
    VecCross,
    VecFromEntity,
    Raycast,
    GetFocalPoint,

    // ── 戰鬥狀態 ─────────────────────────────────────────
    ReadBattleStat,

    // ── 被動反應 ─────────────────────────────────────────
    WaitCondition,

    // ── 補完 ─────────────────────────────────────────────
    QueryNearCount,
    RandomJump,
    EdgeRising,
    EdgeFalling,
    EdgeSinglePulse,

    // ── Phase 4 ───────────────────────────────────────────
    RegisterFilter,
    AnchorSnapshot,
    RollbackSnapshot,
    AlternateJump,
    SetActivationMode,

    // GAS-4: apply GameplayEffect to a target
    ApplyGasEffect,
};
