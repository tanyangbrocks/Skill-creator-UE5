#pragma once
#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "OpCode.h"
#include "Instruction.generated.h"

// ══════════════════════════════════════════════════════════════════
//  OpCode Args Structs
//  每個 EOpCode 對應一個固定 Args struct；型別由 OpCode 隱式決定
//  執行時：Instr.Payload.Get<FDamageArgs>()
//  ⚠️ 不要用 TMap<FString/FName, ...>，opcode 本身已告知型別
// ══════════════════════════════════════════════════════════════════

// Wait / SleepFrames
USTRUCT() struct FWaitArgs      { GENERATED_BODY() UPROPERTY() float Duration = 1.f; };
USTRUCT() struct FSleepArgs     { GENERATED_BODY() UPROPERTY() int32 Frames   = 1;   };

// Jump / JumpIfFalse（TargetPC = 編譯後的指令索引）
USTRUCT() struct FJumpArgs      { GENERATED_BODY() UPROPERTY() int32 TargetPC = 0;   };

// SetVar / GetVar
USTRUCT()
struct FSetVarArgs
{
    GENERATED_BODY()
    UPROPERTY() FName  VarName;
    UPROPERTY() float  Value    = 0.f;
    UPROPERTY() bool   bIsExpr  = false;    // true = Value 來自另一個變數
    UPROPERTY() FName  SrcVar;              // bIsExpr = true 時有效
};

// InvokeTotem
USTRUCT()
struct FInvokeTotemArgs
{
    GENERATED_BODY()
    UPROPERTY() FName TotemId;
    UPROPERTY() FName SlotName;
};

// InvokeSpell（連段）
USTRUCT() struct FInvokeSpellArgs { GENERATED_BODY() UPROPERTY() FName SpellName; };

// RepeatPush（迭代次數）
USTRUCT() struct FRepeatPushArgs  { GENERATED_BODY() UPROPERTY() int32 Count = 1; UPROPERTY() int32 LoopEndPC = 0; };

// RepeatStep（跳回起點或結束）
USTRUCT() struct FRepeatStepArgs  { GENERATED_BODY() UPROPERTY() int32 LoopStartPC = 0; };

// WhileCheck / ForEachStart
USTRUCT() struct FLoopBoundArgs   { GENERATED_BODY() UPROPERTY() int32 LoopEndPC = 0; };

// StoreCompare
USTRUCT()
struct FStoreCompareArgs
{
    GENERATED_BODY()
    UPROPERTY() FName  VarA;
    UPROPERTY() FName  VarB;
    UPROPERTY() FName  Op;          // "==" / "!=" / "<" / "<=" / ">" / ">="
    UPROPERTY() FName  ResultVar;
};

// QueryNearest / QueryNearCount
USTRUCT()
struct FQueryArgs
{
    GENERATED_BODY()
    UPROPERTY() float  Radius     = 5.f;
    UPROPERTY() FName  Faction;         // "enemy" / "ally" / "any"
    UPROPERTY() FName  ResultVar;
    UPROPERTY() int32  LoopEndPC  = 0;  // ForEach 用：空時跳出
};

// GetEntityProp / StoreEntityProp
USTRUCT()
struct FEntityPropArgs
{
    GENERATED_BODY()
    UPROPERTY() FName  PropName;   // "hp" / "maxhp" / "mp" / "maxmp" / "x" / "y"
    UPROPERTY() FName  ResultVar;
    UPROPERTY() float  Value = 0.f; // StoreEntityProp 用
};

// Broadcast / OnReceive
USTRUCT() struct FBroadcastArgs { GENERATED_BODY() UPROPERTY() FName SignalName; };

// List 操作（多用途）
USTRUCT()
struct FListArgs
{
    GENERATED_BODY()
    UPROPERTY() FName  ListName;
    UPROPERTY() FName  ValueVar;    // Append / Set 的來源；Get/Pop/Dequeue 的目標
    UPROPERTY() FName  ResultVar;
    UPROPERTY() int32  Index = 0;   // 1-based（Get / Set / RemoveAt）
};

// 任務計數器
USTRUCT()
struct FTaskCounterArgs
{
    GENERATED_BODY()
    UPROPERTY() FName  CounterName;
    UPROPERTY() float  Value      = 0.f;
    UPROPERTY() FName  ResultVar;
    UPROPERTY() int32  TargetPC   = 0;   // OnReach 跳出地址
};

// 向量 Make
USTRUCT()
struct FVecMakeArgs
{
    GENERATED_BODY()
    UPROPERTY() FName  ResultName;
    UPROPERTY() float  X = 0.f;
    UPROPERTY() float  Y = 0.f;
    UPROPERTY() bool   bFromVars = false; // true = X/Y 從同名 float 變數讀取
    UPROPERTY() FName  XVar;
    UPROPERTY() FName  YVar;
};

// 向量雙目運算（VecAdd / VecSub / VecScale / VecDot / VecCross）
USTRUCT()
struct FVecBinopArgs
{
    GENERATED_BODY()
    UPROPERTY() FName  VecA;
    UPROPERTY() FName  VecB;       // VecScale 時存 scalar 變數名
    UPROPERTY() FName  Result;
};

// 向量單目運算（VecNegate / VecNorm / VecLength / VecGetComp）
USTRUCT()
struct FVecUnopArgs
{
    GENERATED_BODY()
    UPROPERTY() FName  Vec;
    UPROPERTY() FName  Result;
    UPROPERTY() FName  Component;  // VecGetComp 用："x" 或 "y"
};

// Raycast
USTRUCT()
struct FRaycastArgs
{
    GENERATED_BODY()
    UPROPERTY() FName  StartVec;
    UPROPERTY() FName  DirVec;
    UPROPERTY() float  MaxDist  = 20.f;
    UPROPERTY() FName  Result;     // result.x / result.y / result.hit / result.mat
};

// WaitCondition
USTRUCT()
struct FWaitConditionArgs
{
    GENERATED_BODY()
    UPROPERTY() FName  CondType;   // "hpPct" / "mpPct" / "hitReceived"
    UPROPERTY() float  Threshold  = 0.f;
};

// ReadBattleStat / ReadExecStat
USTRUCT()
struct FReadStatArgs
{
    GENERATED_BODY()
    UPROPERTY() FName  StatName;   // "castCount" / "damageDealt" / "killCount" / "loopcastIndex" / "successCount"
    UPROPERTY() FName  ResultVar;
};

// RandomJump（N 個目標地址）
USTRUCT()
struct FRandomJumpArgs
{
    GENERATED_BODY()
    UPROPERTY() TArray<int32> Targets;  // TargetPC 陣列
};

// SetActivationMode
USTRUCT()
struct FSetActivationModeArgs
{
    GENERATED_BODY()
    UPROPERTY() uint8 Mode = 0;    // 0=Instant, 1=Declare, 2=Sustained
};

// RegisterFilter（Phase 4 行動攔截）
USTRUCT()
struct FRegisterFilterArgs
{
    GENERATED_BODY()
    UPROPERTY() FName  FilterType; // "DamageShield" / "DeathGuard"
    UPROPERTY() FName  Mode;       // DamageShield: "cancel" / "half" / "cap"
};

// AnchorSnapshot / RollbackSnapshot（Phase 4 快照）
USTRUCT()
struct FSnapshotArgs
{
    GENERATED_BODY()
    UPROPERTY() float Radius = 5.f;
};

// ══════════════════════════════════════════════════════════════════
//  FInstruction — VM 執行單元
// ══════════════════════════════════════════════════════════════════
USTRUCT()
struct FInstruction
{
    GENERATED_BODY()

    // 指令碼（決定 Payload 的型別）
    UPROPERTY() EOpCode OpCode = EOpCode::Wait;

    // 單一 Payload：型別由 OpCode 隱式決定
    // 使用：Instr.Payload.Get<FWaitArgs>()
    // ⚠️ 不要用 TMap — opcode 已告知型別，直接 Get<T>() 是最優路徑
    UPROPERTY() FInstancedStruct Payload;
};
