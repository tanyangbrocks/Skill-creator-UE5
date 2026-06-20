#include "BlockUIDescriptor.h"
#include "FBlockNode.h"
#include "Instruction.h"
#include "UParamControlWidgets.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

// ══════════════════════════════════════════════════════════════════
//  共用小工具（對應 Godot ScratchCanvas.cs:311-388 SmallEdit/SmallSpin/
//  SmallDrop/CheckBox/TinyLbl 工廠 + ConditionUI）
// ══════════════════════════════════════════════════════════════════

static void AddW(UHorizontalBox* Row, UWidget* W)
{
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(W))
        S->SetVerticalAlignment(VAlign_Center);
}

static void Tiny(UWidget* Ctx, UHorizontalBox* Row, const FString& Text)
{
    UParamTinyLabelWidget* W = CreateWidget<UParamTinyLabelWidget>(Ctx);
    W->Setup(FText::FromString(Text));
    AddW(Row, W);
}

static void TextEdit(UWidget* Ctx, UHorizontalBox* Row, const FString& Initial, const FString& Hint,
                      float Width, TFunction<void(const FString&)> OnCommit)
{
    UParamTextEditWidget* W = CreateWidget<UParamTextEditWidget>(Ctx);
    W->Setup(Initial, FText::FromString(Hint), Width, MoveTemp(OnCommit));
    AddW(Row, W);
}

static void Spin(UWidget* Ctx, UHorizontalBox* Row, float Initial, float Min, float Max, float Step,
                  float Width, TFunction<void(float)> OnChanged)
{
    UParamSpinWidget* W = CreateWidget<UParamSpinWidget>(Ctx);
    W->Setup(Initial, Min, Max, Step, Width, MoveTemp(OnChanged));
    AddW(Row, W);
}

static void Dropdown(UWidget* Ctx, UHorizontalBox* Row, const TArray<FString>& Values,
                      const TArray<FString>& Labels, const FString& Current, float Width,
                      TFunction<void(const FString&)> OnChanged)
{
    UParamDropdownWidget* W = CreateWidget<UParamDropdownWidget>(Ctx);
    W->Setup(Values, Labels, Current, Width, MoveTemp(OnChanged));
    AddW(Row, W);
}

static void Check(UWidget* Ctx, UHorizontalBox* Row, bool Initial, const FString& Label,
                   TFunction<void(bool)> OnChanged)
{
    UParamCheckboxWidget* W = CreateWidget<UParamCheckboxWidget>(Ctx);
    W->Setup(Initial, FText::FromString(Label), MoveTemp(OnChanged));
    AddW(Row, W);
}

// FNumRef：顯示 Var 名稱（若有）否則顯示 Val 數值；提交時數字字串→Val，否則當變數名
// （對應 Godot 同個 SmallEdit 欄位身兼字面值/變數名雙重語意，例如 "duration"/"count"）
static void NumRefEdit(UWidget* Ctx, UHorizontalBox* Row, FNumRef* Ref, const FString& Hint, float Width)
{
    const FString Initial = Ref->Var.IsNone() ? FString::SanitizeFloat(Ref->Val) : Ref->Var.ToString();
    TextEdit(Ctx, Row, Initial, Hint, Width, [Ref](const FString& Text)
    {
        if (Text.IsNumeric()) { Ref->Val = FCString::Atof(*Text); Ref->Var = NAME_None; }
        else                  { Ref->Var = FName(*Text); }
    });
}

// 取出/建立 Block.Params["args"] 的 T，回傳可修改指標（找不到時建立預設值並寫回）
template<typename T>
static T* GetOrAddArgs(FBlockNode& Block)
{
    FInstancedStruct& IS = Block.Params.FindOrAdd(TEXT("args"));
    if (!IS.IsValid() || !IS.GetPtr<T>())
        IS = FInstancedStruct::Make<T>(T{});
    return IS.GetMutablePtr<T>();
}

static FConditionArgs* GetOrAddCond(FBlockNode& Block)
{
    FInstancedStruct& IS = Block.Params.FindOrAdd(TEXT("cond"));
    if (!IS.IsValid() || !IS.GetPtr<FConditionArgs>())
        IS = FInstancedStruct::Make<FConditionArgs>(FConditionArgs{});
    return IS.GetMutablePtr<FConditionArgs>();
}

// ConditionUI（對應 Godot ScratchCanvas.cs ConditionUI，被 If/RepeatWhile/Evaluate/
// RisingEdge/FallingEdge/SinglePulse 六種積木共用）。conditionType 改變時呼叫
// OnStructuralChange——因為切換後要顯示的子控制項不同（Compare 三欄 vs VarBool 一欄
// vs Totem* 一欄），必須重建這一列。
static void ConditionUI(UWidget* Ctx, UHorizontalBox* Row, FBlockNode& Block,
                         const TFunction<void()>& OnStructuralChange)
{
    FConditionArgs* Cond = GetOrAddCond(Block);

    static const TArray<FString> TypeValues = { TEXT("TotemDone"), TEXT("TotemHit"), TEXT("TotemFizzle"), TEXT("Compare"), TEXT("VarBool") };
    static const TArray<FString> TypeLabels = { TEXT("技能完成"), TEXT("技能命中"), TEXT("技能失效"), TEXT("數值比較"), TEXT("布林變數") };
    const FString CurType = TypeValues[FMath::Clamp((int32)Cond->Type, 0, 4)];

    Dropdown(Ctx, Row, TypeValues, TypeLabels, CurType, 72.f, [Cond, OnStructuralChange](const FString& V)
    {
        if      (V == TEXT("TotemDone"))   Cond->Type = EConditionType::TotemDone;
        else if (V == TEXT("TotemHit"))    Cond->Type = EConditionType::TotemHit;
        else if (V == TEXT("TotemFizzle")) Cond->Type = EConditionType::TotemFizzle;
        else if (V == TEXT("Compare"))     Cond->Type = EConditionType::Compare;
        else if (V == TEXT("VarBool"))     Cond->Type = EConditionType::VarBool;
        if (OnStructuralChange) OnStructuralChange();
    });

    if (Cond->Type == EConditionType::Compare)
    {
        NumRefEdit(Ctx, Row, &Cond->Left, TEXT("左值"), 44.f);
        static const TArray<FString> Ops = { TEXT("=="), TEXT("!="), TEXT("<"), TEXT("<="), TEXT(">"), TEXT(">=") };
        static const TArray<FString> OpLabels = { TEXT(">"), TEXT("<"), TEXT("="), TEXT("≠"), TEXT(">="), TEXT("<=") };
        Dropdown(Ctx, Row, Ops, Ops, Cond->Op.IsNone() ? TEXT("==") : Cond->Op.ToString(), 40.f,
            [Cond](const FString& V) { Cond->Op = FName(*V); });
        NumRefEdit(Ctx, Row, &Cond->Right, TEXT("右值"), 44.f);
    }
    else if (Cond->Type == EConditionType::VarBool)
    {
        TextEdit(Ctx, Row, Cond->BoolVar.ToString(), TEXT("變數名"), 72.f,
            [Cond](const FString& T) { Cond->BoolVar = FName(*T); });
    }
    else // TotemDone / TotemHit / TotemFizzle
    {
        TextEdit(Ctx, Row, Cond->TotemName.ToString(), TEXT("技能因子名"), 90.f,
            [Cond](const FString& T) { Cond->TotemName = FName(*T); });
    }
}

// ══════════════════════════════════════════════════════════════════
//  顏色常數（對應 Godot ScratchCanvas.cs:429-442，逐個精確值，取代
//  Phase 2/3 的「每類別一色」近似版）
// ══════════════════════════════════════════════════════════════════
static const FLinearColor COrng (1.00f, 0.72f, 0.35f);
static const FLinearColor CFlow (0.65f, 0.95f, 0.30f);
static const FLinearColor CGrn  (0.38f, 0.88f, 0.48f);
static const FLinearColor CCyan (0.38f, 0.88f, 0.88f);
static const FLinearColor CYlw  (1.00f, 0.88f, 0.28f);
static const FLinearColor COrnD (1.00f, 0.65f, 0.20f);
static const FLinearColor CBlue (0.55f, 0.80f, 1.00f);
static const FLinearColor CPurp (0.80f, 0.38f, 1.00f);
static const FLinearColor CRed  (1.00f, 0.42f, 0.42f);
static const FLinearColor CLvnd (0.95f, 0.65f, 0.95f);
static const FLinearColor CVec  (0.30f, 0.88f, 0.80f);
static const FLinearColor CGray (0.75f, 0.75f, 0.75f);
static const FLinearColor CBattleStat (0.15f, 0.55f, 0.55f);
static const FLinearColor CIntercept  (0.35f, 0.78f, 0.95f);
static const FLinearColor CSnapshot   (0.72f, 0.28f, 0.95f);
static const FLinearColor CEffectTag  (0.40f, 0.65f, 0.45f);

// ══════════════════════════════════════════════════════════════════
//  集中表建構（對應 Godot ScratchCanvas.cs:474-850 `_descs`）
// ══════════════════════════════════════════════════════════════════

static TMap<EBlockType, FBlockUIDescriptor> BuildRegistry()
{
    TMap<EBlockType, FBlockUIDescriptor> M;
    auto Reg = [&](EBlockType T, FLinearColor C, const TCHAR* Name,
                   TFunction<void(UWidget*, UHorizontalBox*, FBlockNode&, const TFunction<void()>&)> UI)
    {
        FBlockUIDescriptor D;
        D.Color = C;
        D.DisplayName = FText::FromString(Name);
        D.BuildParamsUI = MoveTemp(UI);
        M.Add(T, MoveTemp(D));
    };

    // ── 技能呼叫 ── Godot ScratchCanvas.cs:477-480
    Reg(EBlockType::InvokeSpell, COrng, TEXT("施放其他技能整構"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FInvokeSpellArgs* A = GetOrAddArgs<FInvokeSpellArgs>(B);
            TextEdit(Ctx, Row, A->SpellName.ToString(), TEXT("整構名"), 90.f,
                [A](const FString& T) { A->SpellName = FName(*T); });
        });
    Reg(EBlockType::InvokeTotem, COrng, TEXT("使用技能因子（依插槽）"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            // Godot 用 TotemDropZone（拖拉技能因子綁定插槽）；UE5 暫用插槽名稱文字輸入
            // 取代（FInvokeTotemArgs.SlotRef 直接對應 FSpellArray.Slots 具名參照）
            FInvokeTotemArgs* A = GetOrAddArgs<FInvokeTotemArgs>(B);
            TextEdit(Ctx, Row, A->SlotRef.ToString(), TEXT("插槽名"), 80.f,
                [A](const FString& T) { A->SlotRef = FName(*T); });
        });

    // ── 控制流 ── Godot ScratchCanvas.cs:483-496
    Reg(EBlockType::If, CFlow, TEXT("如果"), ConditionUI);
    Reg(EBlockType::RepeatN, CFlow, TEXT("重複 N 次"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FRepeatPushArgs* A = GetOrAddArgs<FRepeatPushArgs>(B);
            NumRefEdit(Ctx, Row, &A->Count, TEXT("次數/變數"), 60.f);
            Tiny(Ctx, Row, TEXT("次"));
        });
    Reg(EBlockType::RepeatWhile, CFlow, TEXT("條件成立，重複"), ConditionUI);
    Reg(EBlockType::RandomChoice, CFlow, TEXT("隨機選擇"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        { Tiny(Ctx, Row, TEXT("兩支各 50%")); });
    Reg(EBlockType::ForEachNearby, CFlow, TEXT("對每個附近敵人"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FQueryArgs* A = GetOrAddArgs<FQueryArgs>(B);
            NumRefEdit(Ctx, Row, &A->Radius, TEXT("半徑"), 40.f);
            Tiny(Ctx, Row, TEXT("格內"));
        });
    Reg(EBlockType::Evaluate, CFlow, TEXT("條件成立執行（無 ELSE）"), ConditionUI);
    Reg(EBlockType::Die, CRed, TEXT("終止技能整構"), nullptr);
    Reg(EBlockType::SequentialGate, CFlow, TEXT("按順序輪流執行"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        { Tiny(Ctx, Row, TEXT("依序輪流（無額外參數）")); });
    Reg(EBlockType::AlternateTrigger, CFlow, TEXT("奇/偶次輪流執行"), nullptr);

    // ── 時序 ── Godot ScratchCanvas.cs:499-502
    Reg(EBlockType::Wait, CGrn, TEXT("等待"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FWaitArgs* A = GetOrAddArgs<FWaitArgs>(B);
            NumRefEdit(Ctx, Row, &A->Duration, TEXT("秒數/變數"), 60.f);
            Tiny(Ctx, Row, TEXT("秒"));
        });
    Reg(EBlockType::Sleep, CGrn, TEXT("等待 N 幀"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FSleepArgs* A = GetOrAddArgs<FSleepArgs>(B);
            NumRefEdit(Ctx, Row, &A->Frames, TEXT("幀數/變數"), 60.f);
            Tiny(Ctx, Row, TEXT("幀"));
        });
    Reg(EBlockType::EndOfChain, CGrn, TEXT("連擊結束時"), nullptr);

    // ── 觸發時機 ── Godot ScratchCanvas.cs:505-513
    Reg(EBlockType::RisingEdge, CCyan, TEXT("條件剛成立時"), ConditionUI);
    Reg(EBlockType::FallingEdge, CCyan, TEXT("條件剛結束時"), ConditionUI);
    Reg(EBlockType::SinglePulse, CCyan, TEXT("條件首次成立時"), ConditionUI);

    // ── 變數 ── Godot ScratchCanvas.cs:516-529 + GetComboCount
    Reg(EBlockType::SetVar, CYlw, TEXT("設定變數"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FSetVarArgs* A = GetOrAddArgs<FSetVarArgs>(B);
            TextEdit(Ctx, Row, A->VarName.ToString(), TEXT("變數名"), 52.f,
                [A](const FString& T) { A->VarName = FName(*T); });
            Tiny(Ctx, Row, TEXT("="));
            NumRefEdit(Ctx, Row, &A->Value, TEXT("值"), 52.f);
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::GetVar, CYlw, TEXT("讀取變數"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FSetVarArgs* A = GetOrAddArgs<FSetVarArgs>(B);
            TextEdit(Ctx, Row, A->VarName.ToString(), TEXT("變數名"), 72.f,
                [A](const FString& T) { A->VarName = FName(*T); });
        });
    Reg(EBlockType::SetVarBool, CYlw, TEXT("設定布林值"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FSetVarArgs* A = GetOrAddArgs<FSetVarArgs>(B);
            TextEdit(Ctx, Row, A->VarName.ToString(), TEXT("變數名"), 52.f,
                [A](const FString& T) { A->VarName = FName(*T); });
            Tiny(Ctx, Row, TEXT("="));
            Dropdown(Ctx, Row, { TEXT("1"), TEXT("0") }, { TEXT("真"), TEXT("假") },
                A->Value.Val != 0.f ? TEXT("1") : TEXT("0"), 44.f,
                [A](const FString& V) { A->Value.Val = (V == TEXT("1")) ? 1.f : 0.f; A->Value.Var = NAME_None; });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::GetVarBool, CYlw, TEXT("讀取布林值"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FSetVarArgs* A = GetOrAddArgs<FSetVarArgs>(B);
            TextEdit(Ctx, Row, A->VarName.ToString(), TEXT("源變數"), 52.f,
                [A](const FString& T) { A->VarName = FName(*T); });
            Tiny(Ctx, Row, TEXT("→"));
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::Compare, CYlw, TEXT("比較數值"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FStoreCompareArgs* A = GetOrAddArgs<FStoreCompareArgs>(B);
            NumRefEdit(Ctx, Row, &A->Left, TEXT("左值"), 44.f);
            static const TArray<FString> Ops = { TEXT("=="), TEXT("!="), TEXT("<"), TEXT("<="), TEXT(">"), TEXT(">=") };
            Dropdown(Ctx, Row, Ops, Ops, A->Op.IsNone() ? TEXT("==") : A->Op.ToString(), 40.f,
                [A](const FString& V) { A->Op = FName(*V); });
            NumRefEdit(Ctx, Row, &A->Right, TEXT("右值"), 44.f);
            Tiny(Ctx, Row, TEXT("存入"));
            TextEdit(Ctx, Row, A->ResultVar.ToString(), TEXT("變數名"), 56.f,
                [A](const FString& T) { A->ResultVar = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::GetComboCount, CYlw, TEXT("讀取連擊數"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FReadStatArgs* A = GetOrAddArgs<FReadStatArgs>(B);
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->ResultVar.ToString(), TEXT("變數"), 60.f,
                [A](const FString& T) { A->ResultVar = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });

    // ── 執行追蹤 ── Godot：LoopcastIndex / SuccessCount
    Reg(EBlockType::LoopcastIndex, CYlw, TEXT("本陣觸發次數"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FReadStatArgs* A = GetOrAddArgs<FReadStatArgs>(B);
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->ResultVar.ToString(), TEXT("存入變數"), 64.f,
                [A](const FString& T) { A->ResultVar = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::SuccessCount, CYlw, TEXT("命中技能因子數量"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FReadStatArgs* A = GetOrAddArgs<FReadStatArgs>(B);
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->ResultVar.ToString(), TEXT("存入變數"), 64.f,
                [A](const FString& T) { A->ResultVar = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });

    // ── 列表 ── Godot ScratchCanvas.cs（10 種）
    auto ListNameOnly = [](const TCHAR* Name) {
        return [Name](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FListArgs* A = GetOrAddArgs<FListArgs>(B);
            TextEdit(Ctx, Row, A->ListName.ToString(), TEXT("列表名"), 72.f,
                [A](const FString& T) { A->ListName = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        };
    };
    Reg(EBlockType::ListCreate, COrnD, TEXT("建立列表"), ListNameOnly(TEXT("ListCreate")));
    Reg(EBlockType::ListClear,  COrnD, TEXT("清空列表"), ListNameOnly(TEXT("ListClear")));
    Reg(EBlockType::ListAppend, COrnD, TEXT("列表加入末尾"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FListArgs* A = GetOrAddArgs<FListArgs>(B);
            TextEdit(Ctx, Row, A->ListName.ToString(), TEXT("列表名"), 64.f, [A](const FString& T) { A->ListName = FName(*T); });
            Tiny(Ctx, Row, TEXT("+"));
            NumRefEdit(Ctx, Row, &A->Value, TEXT("值/變數"), 60.f);
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::ListPop, COrnD, TEXT("取出列表末尾"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FListArgs* A = GetOrAddArgs<FListArgs>(B);
            TextEdit(Ctx, Row, A->ListName.ToString(), TEXT("列表名"), 64.f, [A](const FString& T) { A->ListName = FName(*T); });
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->ResultVar.ToString(), TEXT("存入變數"), 56.f, [A](const FString& T) { A->ResultVar = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::ListDequeue, COrnD, TEXT("取出列表第一項"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FListArgs* A = GetOrAddArgs<FListArgs>(B);
            TextEdit(Ctx, Row, A->ListName.ToString(), TEXT("列表名"), 64.f, [A](const FString& T) { A->ListName = FName(*T); });
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->ResultVar.ToString(), TEXT("存入變數"), 56.f, [A](const FString& T) { A->ResultVar = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::ListGet, COrnD, TEXT("讀取列表第 N 項"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FListArgs* A = GetOrAddArgs<FListArgs>(B);
            TextEdit(Ctx, Row, A->ListName.ToString(), TEXT("列表名"), 64.f, [A](const FString& T) { A->ListName = FName(*T); });
            Tiny(Ctx, Row, TEXT("第"));
            NumRefEdit(Ctx, Row, &A->Index, TEXT("N"), 40.f);
            Tiny(Ctx, Row, TEXT("項→"));
            TextEdit(Ctx, Row, A->ResultVar.ToString(), TEXT("存入變數"), 52.f, [A](const FString& T) { A->ResultVar = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::ListSet, COrnD, TEXT("修改列表第 N 項"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FListArgs* A = GetOrAddArgs<FListArgs>(B);
            TextEdit(Ctx, Row, A->ListName.ToString(), TEXT("列表名"), 64.f, [A](const FString& T) { A->ListName = FName(*T); });
            Tiny(Ctx, Row, TEXT("第"));
            NumRefEdit(Ctx, Row, &A->Index, TEXT("N"), 36.f);
            Tiny(Ctx, Row, TEXT("項="));
            NumRefEdit(Ctx, Row, &A->Value, TEXT("值/變數"), 56.f);
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::ListLength, COrnD, TEXT("取得列表長度"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FListArgs* A = GetOrAddArgs<FListArgs>(B);
            TextEdit(Ctx, Row, A->ListName.ToString(), TEXT("列表名"), 64.f, [A](const FString& T) { A->ListName = FName(*T); });
            Tiny(Ctx, Row, TEXT("長度→"));
            TextEdit(Ctx, Row, A->ResultVar.ToString(), TEXT("存入變數"), 56.f, [A](const FString& T) { A->ResultVar = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::ListContains, COrnD, TEXT("列表是否包含"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FListArgs* A = GetOrAddArgs<FListArgs>(B);
            TextEdit(Ctx, Row, A->ListName.ToString(), TEXT("列表名"), 64.f, [A](const FString& T) { A->ListName = FName(*T); });
            Tiny(Ctx, Row, TEXT("含"));
            NumRefEdit(Ctx, Row, &A->Value, TEXT("值/變數"), 56.f);
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->ResultVar.ToString(), TEXT("結果變數"), 52.f, [A](const FString& T) { A->ResultVar = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::ListRemoveAt, COrnD, TEXT("刪除列表第 N 項"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FListArgs* A = GetOrAddArgs<FListArgs>(B);
            TextEdit(Ctx, Row, A->ListName.ToString(), TEXT("列表名"), 64.f, [A](const FString& T) { A->ListName = FName(*T); });
            Tiny(Ctx, Row, TEXT("刪第"));
            NumRefEdit(Ctx, Row, &A->Index, TEXT("N"), 40.f);
            Tiny(Ctx, Row, TEXT("項"));
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });

    // ── 敵人查詢 ── Godot ScratchCanvas.cs
    Reg(EBlockType::QueryNear, CBlue, TEXT("附近敵人數量"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FQueryArgs* A = GetOrAddArgs<FQueryArgs>(B);
            NumRefEdit(Ctx, Row, &A->Radius, TEXT("半徑"), 40.f);
            Tiny(Ctx, Row, TEXT("格內→"));
            TextEdit(Ctx, Row, A->ResultVar.ToString(), TEXT("存入變數"), 60.f, [A](const FString& T) { A->ResultVar = FName(*T); });
        });
    Reg(EBlockType::QueryNearest, CBlue, TEXT("最近的敵人"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FQueryArgs* A = GetOrAddArgs<FQueryArgs>(B);
            NumRefEdit(Ctx, Row, &A->Radius, TEXT("半徑"), 40.f);
            Tiny(Ctx, Row, TEXT("格內→"));
            TextEdit(Ctx, Row, A->ResultVar.ToString(), TEXT("變數前綴"), 60.f, [A](const FString& T) { A->ResultVar = FName(*T); });
        });
    Reg(EBlockType::GetEntityProp, CBlue, TEXT("讀取敵人屬性"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FEntityPropArgs* A = GetOrAddArgs<FEntityPropArgs>(B);
            static const TArray<FString> Props = { TEXT("hp"), TEXT("maxhp"), TEXT("x"), TEXT("y"), TEXT("z") };
            static const TArray<FString> PropLabels = { TEXT("生命值"), TEXT("最大生命"), TEXT("X 座標"), TEXT("Y 座標"), TEXT("Z 座標") };
            Dropdown(Ctx, Row, Props, PropLabels, A->PropName.IsNone() ? TEXT("hp") : A->PropName.ToString(), 60.f,
                [A](const FString& V) { A->PropName = FName(*V); });
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->ResultVar.ToString(), TEXT("存入變數"), 64.f, [A](const FString& T) { A->ResultVar = FName(*T); });
        });
    Reg(EBlockType::SetEntityProp, CBlue, TEXT("設定敵人屬性"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>& OnStructuralChange)
        {
            FEntityPropArgs* A = GetOrAddArgs<FEntityPropArgs>(B);
            static const TArray<FString> Props = { TEXT("hp"), TEXT("x"), TEXT("y"), TEXT("z") };
            static const TArray<FString> PropLabels = { TEXT("生命值"), TEXT("X 座標"), TEXT("Y 座標"), TEXT("Z 座標") };
            Dropdown(Ctx, Row, Props, PropLabels, A->PropName.IsNone() ? TEXT("hp") : A->PropName.ToString(), 52.f,
                [A, OnStructuralChange](const FString& V) { A->PropName = FName(*V); if (OnStructuralChange) OnStructuralChange(); });
            // 對應 Godot 動態 UI：hp 顯示「扣除傷害」，x/y/z 顯示「設為」
            Tiny(Ctx, Row, A->PropName == FName(TEXT("hp")) ? TEXT("扣除") : TEXT("設為"));
            NumRefEdit(Ctx, Row, &A->Value, TEXT("值/變數"), 60.f);
        });

    // ── 廣播訊號 ──
    auto BroadcastUI = [](const TCHAR*) {
        return [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FBroadcastArgs* A = GetOrAddArgs<FBroadcastArgs>(B);
            TextEdit(Ctx, Row, A->SignalName.ToString(), TEXT("訊號名"), 80.f,
                [A](const FString& T) { A->SignalName = FName(*T); });
        };
    };
    Reg(EBlockType::Broadcast,        CPurp, TEXT("廣播訊號"),             BroadcastUI(TEXT("")));
    Reg(EBlockType::BroadcastAndWait, CPurp, TEXT("廣播訊號（等同廣播）"), BroadcastUI(TEXT("")));
    Reg(EBlockType::OnReceive,        CPurp, TEXT("收到訊號時"),           BroadcastUI(TEXT("")));

    // ── 向量運算 ── Godot ScratchCanvas.cs（UE5 FVecMakeArgs 多了 Z，補上第三軸控制）
    Reg(EBlockType::VecMake, CVec, TEXT("建立向量"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FVecMakeArgs* A = GetOrAddArgs<FVecMakeArgs>(B);
            TextEdit(Ctx, Row, A->ResultName.ToString(), TEXT("向量名"), 48.f, [A](const FString& T) { A->ResultName = FName(*T); });
            Tiny(Ctx, Row, TEXT("=("));
            NumRefEdit(Ctx, Row, &A->X, TEXT("x"), 36.f); Tiny(Ctx, Row, TEXT(","));
            NumRefEdit(Ctx, Row, &A->Y, TEXT("y"), 36.f); Tiny(Ctx, Row, TEXT(","));
            NumRefEdit(Ctx, Row, &A->Z, TEXT("z"), 36.f); Tiny(Ctx, Row, TEXT(")"));
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::VecGetComp, CVec, TEXT("取向量分量"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FVecUnopArgs* A = GetOrAddArgs<FVecUnopArgs>(B);
            TextEdit(Ctx, Row, A->Vec.ToString(), TEXT("向量名"), 48.f, [A](const FString& T) { A->Vec = FName(*T); });
            Tiny(Ctx, Row, TEXT("."));
            Dropdown(Ctx, Row, { TEXT("x"), TEXT("y"), TEXT("z") }, { TEXT("x"), TEXT("y"), TEXT("z") },
                A->Component.IsNone() ? TEXT("x") : A->Component.ToString(), 36.f,
                [A](const FString& V) { A->Component = FName(*V); });
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->Result.ToString(), TEXT("存入"), 48.f, [A](const FString& T) { A->Result = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    auto VecBinop = [](const TCHAR* Sym) {
        return [Sym](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FVecBinopArgs* A = GetOrAddArgs<FVecBinopArgs>(B);
            TextEdit(Ctx, Row, A->VecA.ToString(), TEXT("A"), 44.f, [A](const FString& T) { A->VecA = FName(*T); });
            Tiny(Ctx, Row, Sym);
            TextEdit(Ctx, Row, A->VecB.ToString(), TEXT("B"), 44.f, [A](const FString& T) { A->VecB = FName(*T); });
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->Result.ToString(), TEXT("結果"), 48.f, [A](const FString& T) { A->Result = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        };
    };
    Reg(EBlockType::VecAdd,   CVec, TEXT("向量加法"),       VecBinop(TEXT("+")));
    Reg(EBlockType::VecSub,   CVec, TEXT("向量減法"),       VecBinop(TEXT("−")));
    Reg(EBlockType::VecDot,   CVec, TEXT("向量點積（內積）"), VecBinop(TEXT("·")));
    Reg(EBlockType::VecCross, CVec, TEXT("向量叉積（外積）"), VecBinop(TEXT("×")));
    Reg(EBlockType::VecScale, CVec, TEXT("向量縮放"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FVecScaleArgs* A = GetOrAddArgs<FVecScaleArgs>(B);
            TextEdit(Ctx, Row, A->Vec.ToString(), TEXT("向量"), 48.f, [A](const FString& T) { A->Vec = FName(*T); });
            Tiny(Ctx, Row, TEXT("×"));
            NumRefEdit(Ctx, Row, &A->Scalar, TEXT("純量"), 40.f);
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->Result.ToString(), TEXT("結果"), 48.f, [A](const FString& T) { A->Result = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::VecNegate, CVec, TEXT("向量反向"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FVecUnopArgs* A = GetOrAddArgs<FVecUnopArgs>(B);
            Tiny(Ctx, Row, TEXT("−"));
            TextEdit(Ctx, Row, A->Vec.ToString(), TEXT("向量"), 48.f, [A](const FString& T) { A->Vec = FName(*T); });
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->Result.ToString(), TEXT("結果"), 48.f, [A](const FString& T) { A->Result = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::VecNorm, CVec, TEXT("向量正規化（轉為單位向量）"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FVecUnopArgs* A = GetOrAddArgs<FVecUnopArgs>(B);
            Tiny(Ctx, Row, TEXT("norm("));
            TextEdit(Ctx, Row, A->Vec.ToString(), TEXT("向量"), 48.f, [A](const FString& T) { A->Vec = FName(*T); });
            Tiny(Ctx, Row, TEXT(")→"));
            TextEdit(Ctx, Row, A->Result.ToString(), TEXT("結果"), 48.f, [A](const FString& T) { A->Result = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::VecLength, CVec, TEXT("向量長度"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FVecUnopArgs* A = GetOrAddArgs<FVecUnopArgs>(B);
            Tiny(Ctx, Row, TEXT("|"));
            TextEdit(Ctx, Row, A->Vec.ToString(), TEXT("向量"), 48.f, [A](const FString& T) { A->Vec = FName(*T); });
            Tiny(Ctx, Row, TEXT("|→"));
            TextEdit(Ctx, Row, A->Result.ToString(), TEXT("存入"), 48.f, [A](const FString& T) { A->Result = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::VecFromEntity, CVec, TEXT("目標位置→向量"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FVecFromEntityArgs* A = GetOrAddArgs<FVecFromEntityArgs>(B);
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->ResultVec.ToString(), TEXT("結果向量"), 64.f, [A](const FString& T) { A->ResultVec = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::FocalPoint, CVec, TEXT("游標所在位置"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FGetFocalPointArgs* A = GetOrAddArgs<FGetFocalPointArgs>(B);
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->ResultVec.ToString(), TEXT("結果向量"), 64.f, [A](const FString& T) { A->ResultVec = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::Raycast, CVec, TEXT("射線投射"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FRaycastArgs* A = GetOrAddArgs<FRaycastArgs>(B);
            Tiny(Ctx, Row, TEXT("從"));
            TextEdit(Ctx, Row, A->StartVec.ToString(), TEXT("起點向量"), 52.f, [A](const FString& T) { A->StartVec = FName(*T); });
            Tiny(Ctx, Row, TEXT("向"));
            TextEdit(Ctx, Row, A->DirVec.ToString(), TEXT("方向向量"), 52.f, [A](const FString& T) { A->DirVec = FName(*T); });
            Tiny(Ctx, Row, TEXT("射"));
            NumRefEdit(Ctx, Row, &A->MaxDist, TEXT("最遠格數"), 44.f);
            Tiny(Ctx, Row, TEXT("格→"));
            TextEdit(Ctx, Row, A->ResultVec.ToString(), TEXT("結果前綴"), 48.f, [A](const FString& T) { A->ResultVec = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });

    // ── 偵測條件（被動觸發）── Godot ScratchCanvas.cs
    Reg(EBlockType::DetectHpThreshold, CRed, TEXT("生命值低於 N%"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FWaitConditionArgs* A = GetOrAddArgs<FWaitConditionArgs>(B);
            Spin(Ctx, Row, A->Threshold, 1.f, 99.f, 1.f, 44.f, [A](float V) { A->Threshold = V; });
            Tiny(Ctx, Row, TEXT("%"));
        });
    Reg(EBlockType::DetectMpThreshold, CRed, TEXT("魔力值低於 N%"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FWaitConditionArgs* A = GetOrAddArgs<FWaitConditionArgs>(B);
            Spin(Ctx, Row, A->Threshold, 1.f, 99.f, 1.f, 44.f, [A](float V) { A->Threshold = V; });
            Tiny(Ctx, Row, TEXT("%"));
        });
    Reg(EBlockType::DetectHitReceived, CRed, TEXT("受到攻擊時"), nullptr);
    Reg(EBlockType::DetectEntityEnter, CRed, TEXT("敵人進入範圍時"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            // ⚠️ 已知簡化：UE5 FWaitConditionArgs 只有 Threshold，沒有 Godot 的獨立
            // radius/faction 欄位，這裡借 Threshold 當半徑用
            FWaitConditionArgs* A = GetOrAddArgs<FWaitConditionArgs>(B);
            Spin(Ctx, Row, A->Threshold, 1.f, 30.f, 1.f, 40.f, [A](float V) { A->Threshold = V; });
            Tiny(Ctx, Row, TEXT("格內有敵人"));
        });
    Reg(EBlockType::DetectProjectile, CRed, TEXT("投射物進入範圍時"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FWaitConditionArgs* A = GetOrAddArgs<FWaitConditionArgs>(B);
            Tiny(Ctx, Row, TEXT("半徑"));
            Spin(Ctx, Row, A->Threshold, 1.f, 30.f, 1.f, 40.f, [A](float V) { A->Threshold = V; });
            Tiny(Ctx, Row, TEXT("格"));
        });
    Reg(EBlockType::DetectAttack, CRed, TEXT("敵方蓄力攻擊時"), nullptr);
    Reg(EBlockType::DetectStatusChange, CRed, TEXT("狀態變化時"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FWaitConditionArgs* A = GetOrAddArgs<FWaitConditionArgs>(B);
            Tiny(Ctx, Row, TEXT("狀態"));
            TextEdit(Ctx, Row, A->CondKey.ToString(), TEXT("狀態名"), 70.f, [A](const FString& T) { A->CondKey = FName(*T); });
        });

    // ── 戰鬥統計查詢 ──
    Reg(EBlockType::GetBattleStat, CBattleStat, TEXT("本場戰鬥統計"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FReadStatArgs* A = GetOrAddArgs<FReadStatArgs>(B);
            static const TArray<FString> Stats = { TEXT("castCount"), TEXT("damageDealt"), TEXT("killCount") };
            static const TArray<FString> StatLabels = { TEXT("施放次數"), TEXT("造成傷害"), TEXT("擊殺數") };
            Dropdown(Ctx, Row, Stats, StatLabels, A->StatName.IsNone() ? TEXT("castCount") : A->StatName.ToString(), 80.f,
                [A](const FString& V) { A->StatName = FName(*V); });
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->ResultVar.ToString(), TEXT("變數名"), 64.f, [A](const FString& T) { A->ResultVar = FName(*T); });
        });

    // ── 任務計數器 ──
    Reg(EBlockType::TaskCounterSet, CLvnd, TEXT("計數器設定值"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FTaskCounterArgs* A = GetOrAddArgs<FTaskCounterArgs>(B);
            TextEdit(Ctx, Row, A->CounterName.ToString(), TEXT("計數器名"), 64.f, [A](const FString& T) { A->CounterName = FName(*T); });
            NumRefEdit(Ctx, Row, &A->Value, TEXT("數值"), 44.f);
        });
    Reg(EBlockType::TaskCounterAdd, CLvnd, TEXT("計數器增加"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FTaskCounterArgs* A = GetOrAddArgs<FTaskCounterArgs>(B);
            TextEdit(Ctx, Row, A->CounterName.ToString(), TEXT("計數器名"), 64.f, [A](const FString& T) { A->CounterName = FName(*T); });
            NumRefEdit(Ctx, Row, &A->Value, TEXT("增量"), 44.f);
        });
    Reg(EBlockType::TaskCounterGet, CLvnd, TEXT("計數器讀值"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FTaskCounterArgs* A = GetOrAddArgs<FTaskCounterArgs>(B);
            TextEdit(Ctx, Row, A->CounterName.ToString(), TEXT("計數器名"), 64.f, [A](const FString& T) { A->CounterName = FName(*T); });
            Tiny(Ctx, Row, TEXT("→"));
            TextEdit(Ctx, Row, A->ResultVar.ToString(), TEXT("存入變數"), 64.f, [A](const FString& T) { A->ResultVar = FName(*T); });
            Check(Ctx, Row, A->bGlobal, TEXT("全域"), [A](bool V) { A->bGlobal = V; });
        });
    Reg(EBlockType::TaskCounterOnReach, CLvnd, TEXT("計數器到達時"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FTaskCounterArgs* A = GetOrAddArgs<FTaskCounterArgs>(B);
            TextEdit(Ctx, Row, A->CounterName.ToString(), TEXT("計數器名"), 64.f, [A](const FString& T) { A->CounterName = FName(*T); });
            NumRefEdit(Ctx, Row, &A->Value, TEXT("目標值"), 44.f);
        });
    Reg(EBlockType::TaskCounterReset, CLvnd, TEXT("計數器歸零"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FTaskCounterArgs* A = GetOrAddArgs<FTaskCounterArgs>(B);
            TextEdit(Ctx, Row, A->CounterName.ToString(), TEXT("計數器名"), 64.f, [A](const FString& T) { A->CounterName = FName(*T); });
        });

    // ── Phase 4（Godot 章節）：行動攔截積木 ──
    Reg(EBlockType::DamageShield, CIntercept, TEXT("攔截下一次受傷"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>& OnStructuralChange)
        {
            FRegisterFilterArgs* A = GetOrAddArgs<FRegisterFilterArgs>(B);
            static const TArray<FString> Modes = { TEXT("cancel"), TEXT("halve"), TEXT("cap") };
            static const TArray<FString> ModeLabels = { TEXT("完全免傷"), TEXT("減半"), TEXT("傷害封頂") };
            Dropdown(Ctx, Row, Modes, ModeLabels, A->Mode.IsNone() ? TEXT("cancel") : A->Mode.ToString(), 72.f,
                [A, OnStructuralChange](const FString& V) { A->Mode = FName(*V); if (OnStructuralChange) OnStructuralChange(); });
            if (A->Mode == FName(TEXT("cap")))
            {
                Tiny(Ctx, Row, TEXT("封頂值"));
                Spin(Ctx, Row, A->CapValue, 1.f, 9999.f, 1.f, 52.f, [A](float V) { A->CapValue = V; });
            }
            Tiny(Ctx, Row, TEXT("門檻≥"));
            Spin(Ctx, Row, A->Threshold, 0.f, 9999.f, 1.f, 52.f, [A](float V) { A->Threshold = V; });
            Check(Ctx, Row, A->bOneShot, TEXT("一次性"), [A](bool V) { A->bOneShot = V; });
        });
    Reg(EBlockType::DeathGuard, CIntercept, TEXT("攔截下一次死亡（存活 1HP）"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FRegisterFilterArgs* A = GetOrAddArgs<FRegisterFilterArgs>(B);
            Check(Ctx, Row, A->bOneShot, TEXT("一次性"), [A](bool V) { A->bOneShot = V; });
        });

    // ── Phase 4（Godot 章節）：狀態快照積木 ──
    Reg(EBlockType::Anchor, CSnapshot, TEXT("錨點刻印（群星 LV50+）"),
        [](UWidget* Ctx, UHorizontalBox* Row, FBlockNode& B, const TFunction<void()>&)
        {
            FSnapshotArgs* A = GetOrAddArgs<FSnapshotArgs>(B);
            Tiny(Ctx, Row, TEXT("半徑"));
            NumRefEdit(Ctx, Row, &A->Radius, TEXT("半徑"), 44.f);
            Tiny(Ctx, Row, TEXT("格"));
        });
    Reg(EBlockType::Rollback, CSnapshot, TEXT("回溯刻印（群星 LV50+）"), nullptr);

    // ── 規劃中積木（UI 已接入，VM 待補）──
    Reg(EBlockType::Discard, CGray, TEXT("捨棄輸出"), nullptr);
    Reg(EBlockType::SetActivationInstant,   COrng, TEXT("設為即時施放"), nullptr);
    Reg(EBlockType::SetActivationDeclare,   COrng, TEXT("設為宣告施放"), nullptr);
    Reg(EBlockType::SetActivationSustained, COrng, TEXT("設為持續施放"), nullptr);
    // EffectLabel/OnEffectStart/OnEffectEnd：⚠️ 已知缺口，UE5 Instruction.h 尚無對應
    // Args struct（Godot 有 "label" 字串參數），暫不接參數 UI，只顯示卡片本體
    Reg(EBlockType::EffectLabel,   CEffectTag, TEXT("效果標籤"), nullptr);
    Reg(EBlockType::OnEffectStart, CEffectTag, TEXT("效果開始時"), nullptr);
    Reg(EBlockType::OnEffectEnd,   CEffectTag, TEXT("效果結束時"), nullptr);

    return M;
}

const FBlockUIDescriptor& FBlockUIRegistry::Get(EBlockType Type)
{
    static const TMap<EBlockType, FBlockUIDescriptor> Registry = BuildRegistry();
    static const FBlockUIDescriptor Fallback = []
    {
        FBlockUIDescriptor D;
        D.Color = FLinearColor(0.6f, 0.6f, 0.6f);
        D.DisplayName = FText::FromString(TEXT("未知積木"));
        return D;
    }();

    if (const FBlockUIDescriptor* Found = Registry.Find(Type))
        return *Found;
    return Fallback;
}
