#pragma once
#include "CoreMinimal.h"
#include "BlockType.generated.h"

// 積木型別（對應 Godot BlockType.cs）
// 設計時積木 AST 節點的型別識別；執行時由 SpellCompiler 轉為 EOpCode 指令序列
UENUM(BlueprintType)
enum class EBlockType : uint8
{
    // ── 控制流 ─────────────────────────────────────────────
    If,
    Evaluate,
    RepeatN,
    RepeatWhile,
    ForEachNearby,
    Wait,
    Sleep,
    Die,
    RandomChoice,
    SequentialGate,

    // ── 邊緣觸發 ──────────────────────────────────────────
    RisingEdge,
    FallingEdge,
    AlternateTrigger,
    SinglePulse,

    // ── 捨棄 ─────────────────────────────────────────────
    Discard,

    // ── 效果標記 ─────────────────────────────────────────
    EffectLabel,
    OnEffectStart,
    OnEffectEnd,

    // ── 呼叫 ─────────────────────────────────────────────
    InvokeSpell,
    InvokeTotem,

    // ── 技能因子狀態查詢 ──────────────────────────────────
    TotemDone,
    TotemHit,
    TotemFizzle,

    // ── 發動類型切換 ─────────────────────────────────────
    SetActivationInstant,
    SetActivationDeclare,
    SetActivationSustained,

    // ── 數值變數 ─────────────────────────────────────────
    SetVar,
    GetVar,
    Compare,

    // ── 布林變數 ─────────────────────────────────────────
    SetVarBool,
    GetVarBool,

    // ── 列表 ─────────────────────────────────────────────
    ListCreate,
    ListAppend,
    ListPop,
    ListDequeue,
    ListGet,
    ListSet,
    ListLength,
    ListContains,
    ListRemoveAt,
    ListClear,

    // ── 任務計數器 ────────────────────────────────────────
    TaskCounterSet,
    TaskCounterAdd,
    TaskCounterGet,
    TaskCounterOnReach,
    TaskCounterReset,

    // ── 實體查詢 ─────────────────────────────────────────
    QueryNear,
    QueryNearest,
    GetEntityProp,
    SetEntityProp,

    // ── 廣播通訊 ─────────────────────────────────────────
    Broadcast,
    BroadcastAndWait,
    OnReceive,

    // ── 執行追蹤 ─────────────────────────────────────────
    LoopcastIndex,
    SuccessCount,

    // ── 戰鬥統計 ─────────────────────────────────────────
    GetBattleStat,
    GetComboCount,

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
    FocalPoint,

    // ── 被動反應觸發 ─────────────────────────────────────
    DetectProjectile,
    DetectAttack,
    DetectHitReceived,
    DetectHpThreshold,
    DetectMpThreshold,
    DetectEntityEnter,
    DetectStatusChange,

    // ── 觸發時機 ─────────────────────────────────────────
    EndOfChain,

    // ── Phase 4：行動攔截 ─────────────────────────────────
    DamageShield,
    DeathGuard,

    // ── Phase 4：狀態快照 ─────────────────────────────────
    Anchor,
    Rollback,

    // ── 技能因子／刻印 ────────────────────────────────────
    Totem,
    Engraving,
};
