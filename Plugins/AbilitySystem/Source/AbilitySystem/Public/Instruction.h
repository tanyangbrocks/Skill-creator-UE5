#pragma once
#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "OpCode.h"
#include "Instruction.generated.h"

// ══════════════════════════════════════════════════════════════════
//  FNumRef — 可解析數值：字面 float 或命名變數
//  Var.IsNone() → 使用 Val；否則在 InstanceVars/GlobalVars 查 Var
// ══════════════════════════════════════════════════════════════════
USTRUCT()
struct FNumRef
{
    GENERATED_BODY()
    UPROPERTY() float Val = 0.f;
    UPROPERTY() FName Var;

    static FNumRef Lit(float V)  { FNumRef R; R.Val = V; return R; }
    static FNumRef Ref(FName V)  { FNumRef R; R.Var = V; return R; }
};

// ══════════════════════════════════════════════════════════════════
//  EConditionType / FConditionArgs
//  用於 JumpIfFalse / WhileCheck / Edge* 的條件評估
// ══════════════════════════════════════════════════════════════════
UENUM()
enum class EConditionType : uint8
{
    Compare     = 0,    // Left op Right（op = "==" / "!=" / "<" / "<=" / ">" / ">="）
    VarBool     = 1,    // BoolVar 對應的 float 變數非零即 true
    TotemHit    = 2,
    TotemDone   = 3,
    TotemFizzle = 4,
};

USTRUCT()
struct FConditionArgs
{
    GENERATED_BODY()

    UPROPERTY() EConditionType Type = EConditionType::Compare;

    // Compare 用
    UPROPERTY() FNumRef Left;
    UPROPERTY() FNumRef Right;
    UPROPERTY() FName   Op;         // "==" / "!=" / "<" / "<=" / ">" / ">="

    // TotemHit / TotemDone / TotemFizzle 用
    UPROPERTY() FName TotemName;

    // VarBool 用（變數名稱，非零 = true）
    UPROPERTY() FName BoolVar;
};

// ══════════════════════════════════════════════════════════════════
//  OpCode Args Structs
//  每個 EOpCode 對應一個固定 Args struct；型別由 OpCode 隱式決定
//  讀取：Instr.Payload.GetPtr<FWaitArgs>()
// ══════════════════════════════════════════════════════════════════

// Wait
USTRUCT()
struct FWaitArgs
{
    GENERATED_BODY()
    UPROPERTY() FNumRef Duration;
};

// SleepFrames
USTRUCT()
struct FSleepArgs
{
    GENERATED_BODY()
    UPROPERTY() FNumRef Frames;
};

// Jump（無條件跳轉）
USTRUCT()
struct FJumpArgs
{
    GENERATED_BODY()
    UPROPERTY() int32 TargetPC = 0;
};

// JumpIfFalse（條件跳轉；條件 false 時跳到 TargetPC）
USTRUCT()
struct FJumpIfArgs
{
    GENERATED_BODY()
    UPROPERTY() int32          TargetPC = 0;
    UPROPERTY() FConditionArgs Cond;
};

// SetVar（含 GetVar / SetVarBool / GetVarBool 複用語義）
USTRUCT()
struct FSetVarArgs
{
    GENERATED_BODY()
    UPROPERTY() FName   VarName;
    UPROPERTY() FNumRef Value;      // Var.IsNone() → 字面數值；否則複製來源變數
    UPROPERTY() bool    bGlobal = false;
};

// InvokeTotem
USTRUCT()
struct FInvokeTotemArgs
{
    GENERATED_BODY()
    UPROPERTY() FName SlotRef;      // SpellArray.Slots 具名參照
};

// InvokeSpell（連段）
USTRUCT()
struct FInvokeSpellArgs
{
    GENERATED_BODY()
    UPROPERTY() FName SpellName;
};

// RepeatPush（推入 RepeatN 迭代次數）
USTRUCT()
struct FRepeatPushArgs
{
    GENERATED_BODY()
    UPROPERTY() FNumRef Count;
};

// RepeatStep（遞減計數，>0 跳回 LoopStartPC）
USTRUCT()
struct FRepeatStepArgs
{
    GENERATED_BODY()
    UPROPERTY() int32 LoopStartPC = 0;
};

// WhileCheck（條件 false 跳到 LoopEndPC）
USTRUCT()
struct FWhileArgs
{
    GENERATED_BODY()
    UPROPERTY() int32          LoopEndPC = 0;
    UPROPERTY() FConditionArgs Cond;
};

// ForEachStart / QueryNearest / QueryNearCount（共用 Radius + ResultVar）
USTRUCT()
struct FQueryArgs
{
    GENERATED_BODY()
    UPROPERTY() FNumRef Radius;
    UPROPERTY() FName   ResultVar;
    UPROPERTY() int32   LoopEndPC = 0;  // ForEachStart 專用
};

// ForEachStep（前進 ForEach 迭代器）
USTRUCT()
struct FForEachStepArgs
{
    GENERATED_BODY()
    UPROPERTY() int32 LoopStartPC = 0;
};

// StoreCompare（Compare 積木：儲存比較結果到變數）
USTRUCT()
struct FStoreCompareArgs
{
    GENERATED_BODY()
    UPROPERTY() FNumRef Left;
    UPROPERTY() FNumRef Right;
    UPROPERTY() FName   Op;
    UPROPERTY() FName   ResultVar;
    UPROPERTY() bool    bGlobal = false;
};

// GetEntityProp / StoreEntityProp
USTRUCT()
struct FEntityPropArgs
{
    GENERATED_BODY()
    UPROPERTY() FName   PropName;   // "hp" / "maxhp" / "x" / "y" / "z"
    UPROPERTY() FName   ResultVar;
    UPROPERTY() FNumRef Value;      // StoreEntityProp 用
};

// Broadcast / OnReceive
USTRUCT()
struct FBroadcastArgs
{
    GENERATED_BODY()
    UPROPERTY() FName SignalName;
};

// 列表操作（多用途）
USTRUCT()
struct FListArgs
{
    GENERATED_BODY()
    UPROPERTY() FName   ListName;
    UPROPERTY() FNumRef Value;      // Append / Set 的來源（支援字面或變數）
    UPROPERTY() FName   ResultVar;  // Get / Pop / Dequeue 的目標
    UPROPERTY() FNumRef Index;      // 1-based（Get / Set / RemoveAt）
    UPROPERTY() bool    bGlobal = false;
};

// 任務計數器
USTRUCT()
struct FTaskCounterArgs
{
    GENERATED_BODY()
    UPROPERTY() FName   CounterName;
    UPROPERTY() FNumRef Value;
    UPROPERTY() FName   ResultVar;
    UPROPERTY() int32   TargetPC  = 0;  // OnReach：條件不滿足時跳出
    UPROPERTY() bool    bGlobal   = false;
};

// 向量 Make（3D）
USTRUCT()
struct FVecMakeArgs
{
    GENERATED_BODY()
    UPROPERTY() FName   ResultName;
    UPROPERTY() FNumRef X;
    UPROPERTY() FNumRef Y;
    UPROPERTY() FNumRef Z;
    UPROPERTY() bool    bGlobal = false;
};

// 向量雙目運算（VecAdd / VecSub / VecDot / VecCross）
// VecDot：Result 為純量變數；其餘為向量名稱
USTRUCT()
struct FVecBinopArgs
{
    GENERATED_BODY()
    UPROPERTY() FName VecA;
    UPROPERTY() FName VecB;
    UPROPERTY() FName Result;
    UPROPERTY() bool  bGlobal = false;
};

// 向量縮放（VecScale — scalar 支援字面值或變數）
USTRUCT()
struct FVecScaleArgs
{
    GENERATED_BODY()
    UPROPERTY() FName   Vec;
    UPROPERTY() FNumRef Scalar;
    UPROPERTY() FName   Result;
    UPROPERTY() bool    bGlobal = false;
};

// 向量單目運算（VecNegate / VecNorm / VecLength / VecGetComp）
// VecLength / VecGetComp：Result 為純量；VecNegate / VecNorm：Result 為向量名稱
// Component："x" / "y" / "z"（VecGetComp 用）
USTRUCT()
struct FVecUnopArgs
{
    GENERATED_BODY()
    UPROPERTY() FName Vec;
    UPROPERTY() FName Result;
    UPROPERTY() FName Component;
    UPROPERTY() bool  bGlobal = false;
};

// GetFocalPoint（滑鼠/準心格座標 → 向量）
USTRUCT()
struct FGetFocalPointArgs
{
    GENERATED_BODY()
    UPROPERTY() FName ResultVec;
    UPROPERTY() bool  bGlobal = false;
};

// VecFromEntity（從當前迭代實體位置建立向量）
USTRUCT()
struct FVecFromEntityArgs
{
    GENERATED_BODY()
    UPROPERTY() FName ResultVec;
    UPROPERTY() bool  bGlobal = false;
};

// Raycast
USTRUCT()
struct FRaycastArgs
{
    GENERATED_BODY()
    UPROPERTY() FName   StartVec;
    UPROPERTY() FName   DirVec;
    UPROPERTY() FNumRef MaxDist;
    UPROPERTY() FName   ResultVec;  // result.x/y/z / result.hit / result.mat
    UPROPERTY() bool    bGlobal = false;
};

// WaitCondition（被動觸發：等待條件成立）
USTRUCT()
struct FWaitConditionArgs
{
    GENERATED_BODY()
    UPROPERTY() FName CondKey;      // "hpPct" / "mpPct" / "damaged" / "entityInRange"
    UPROPERTY() float Threshold = 0.f;
};

// ReadBattleStat / ReadExecStat
USTRUCT()
struct FReadStatArgs
{
    GENERATED_BODY()
    UPROPERTY() FName StatName;
    UPROPERTY() FName ResultVar;
    UPROPERTY() bool  bGlobal = false;
};

// RandomJump（從 Targets 陣列隨機選一個 PC）
USTRUCT()
struct FRandomJumpArgs
{
    GENERATED_BODY()
    UPROPERTY() TArray<int32> Targets;
};

// AlternateJump（偶次呼叫 → EvenPC；奇次 → OddPC）
USTRUCT()
struct FAlternateJumpArgs
{
    GENERATED_BODY()
    UPROPERTY() int32 EvenPC = 0;
    UPROPERTY() int32 OddPC  = 0;
};

// SetActivationMode（0=Instant / 1=Declare / 2=Sustained）
USTRUCT()
struct FSetActivationModeArgs
{
    GENERATED_BODY()
    UPROPERTY() uint8 Mode = 0;
};

// RegisterFilter（Phase 4 行動攔截）
USTRUCT()
struct FRegisterFilterArgs
{
    GENERATED_BODY()
    UPROPERTY() FName  FilterType;   // "DamageShield" / "DeathGuard"
    UPROPERTY() FName  Mode;         // "cancel" / "halve" / "cap"
    UPROPERTY() bool   bOneShot   = true;
    UPROPERTY() float  Threshold  = 0.f;
    UPROPERTY() float  CapValue   = 1.f;
};

// AnchorSnapshot / RollbackSnapshot（Phase 4 快照）
USTRUCT()
struct FSnapshotArgs
{
    GENERATED_BODY()
    UPROPERTY() FNumRef Radius;
};

// EdgeRising / EdgeFalling / EdgeSinglePulse
// TargetPC：SinglePulse 用（ThenBranch 結尾地址）；EdgeRising/Falling 忽略
USTRUCT()
struct FEdgeArgs
{
    GENERATED_BODY()
    UPROPERTY() int32          TargetPC = 0;
    UPROPERTY() FConditionArgs Cond;
};

// ══════════════════════════════════════════════════════════════════
//  FInstruction — VM 執行單元
// ══════════════════════════════════════════════════════════════════
USTRUCT()
struct FInstruction
{
    GENERATED_BODY()

    UPROPERTY() EOpCode OpCode = EOpCode::Wait;

    // 型別由 OpCode 隱式決定；讀取：Instr.Payload.GetPtr<FWaitArgs>()
    UPROPERTY() FInstancedStruct Payload;
};
