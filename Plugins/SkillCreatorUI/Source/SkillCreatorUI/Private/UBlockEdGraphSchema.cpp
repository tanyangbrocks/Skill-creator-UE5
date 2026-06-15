#include "UBlockEdGraphSchema.h"
#include "UBlockEdGraphNode.h"
#include "UBlockEdGraph.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "GraphEditorSettings.h"

const FName UBlockEdGraphSchema::ExecCategory = TEXT("exec");

// ── Context menu action ──────────────────────────────────────────────────────

UEdGraphNode* FBlockSchemaAction::PerformAction(UEdGraph* ParentGraph,
                                                 UEdGraphPin* FromPin,
                                                 const FVector2f& Location,
                                                 bool bSelectNewNode)
{
    UBlockEdGraphNode* Node = NewObject<UBlockEdGraphNode>(
        ParentGraph, UBlockEdGraphNode::StaticClass(), NAME_None, RF_Transactional);
    Node->BlockType = BlockTypeToCreate;
    Node->NodePosX  = static_cast<int32>(Location.X);
    Node->NodePosY  = static_cast<int32>(Location.Y);
    Node->AllocateDefaultPins();
    ParentGraph->AddNode(Node, true, bSelectNewNode);

    // Auto-wire from the dragged pin if direction matches.
    if (FromPin && FromPin->Direction == EGPD_Output)
    {
        if (UEdGraphPin* InPin = Node->FindPin(TEXT("In"), EGPD_Input))
            ParentGraph->GetSchema()->TryCreateConnection(FromPin, InPin);
    }
    else if (FromPin && FromPin->Direction == EGPD_Input)
    {
        if (UEdGraphPin* ThenPin = Node->FindPin(TEXT("Then"), EGPD_Output))
            ParentGraph->GetSchema()->TryCreateConnection(ThenPin, FromPin);
    }

    return Node;
}

UEdGraphNode* FBlockSchemaAction::PerformAction(UEdGraph* ParentGraph,
                                                 TArray<UEdGraphPin*>& FromPins,
                                                 const FVector2f& Location,
                                                 bool bSelectNewNode)
{
    UEdGraphPin* FromPin = FromPins.IsEmpty() ? nullptr : FromPins[0];
    return PerformAction(ParentGraph, FromPin, Location, bSelectNewNode);
}

// ── Schema ───────────────────────────────────────────────────────────────────

void UBlockEdGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
    // Helper lambda adds one entry per block type.
    auto Add = [&](EBlockType T, FText Cat, FText Label)
    {
        ContextMenuBuilder.AddAction(MakeShared<FBlockSchemaAction>(
            T, Cat, Label, FText::GetEmpty(), 0));
    };

    const FText CtrlFlow  = NSLOCTEXT("BlockEditor","Cat_Ctrl","Control Flow");
    const FText Variables  = NSLOCTEXT("BlockEditor","Cat_Var","Variables");
    const FText Invocation = NSLOCTEXT("BlockEditor","Cat_Inv","Invocations");
    const FText Timing     = NSLOCTEXT("BlockEditor","Cat_Tim","Timing");
    const FText Query      = NSLOCTEXT("BlockEditor","Cat_Qry","Entity Query");
    const FText Edge       = NSLOCTEXT("BlockEditor","Cat_Edg","Edge Triggers");
    const FText Broadcast  = NSLOCTEXT("BlockEditor","Cat_Brd","Broadcast");
    const FText Vector     = NSLOCTEXT("BlockEditor","Cat_Vec","Vector Math");
    const FText Advanced   = NSLOCTEXT("BlockEditor","Cat_Adv","Advanced");

    Add(EBlockType::If,               CtrlFlow,  NSLOCTEXT("BlockEditor","N_If",           "If"));
    Add(EBlockType::Evaluate,         CtrlFlow,  NSLOCTEXT("BlockEditor","N_Eval",         "Evaluate"));
    Add(EBlockType::RepeatN,          CtrlFlow,  NSLOCTEXT("BlockEditor","N_RepN",         "Repeat N"));
    Add(EBlockType::RepeatWhile,      CtrlFlow,  NSLOCTEXT("BlockEditor","N_RepW",         "Repeat While"));
    Add(EBlockType::ForEachNearby,    CtrlFlow,  NSLOCTEXT("BlockEditor","N_ForE",         "For Each Nearby"));
    Add(EBlockType::RandomChoice,     CtrlFlow,  NSLOCTEXT("BlockEditor","N_Rand",         "Random Choice"));
    Add(EBlockType::SequentialGate,   CtrlFlow,  NSLOCTEXT("BlockEditor","N_SeqG",         "Sequential Gate"));
    Add(EBlockType::Die,              CtrlFlow,  NSLOCTEXT("BlockEditor","N_Die",          "Die"));

    Add(EBlockType::Wait,             Timing,    NSLOCTEXT("BlockEditor","N_Wait",         "Wait"));
    Add(EBlockType::Sleep,            Timing,    NSLOCTEXT("BlockEditor","N_Sleep",        "Sleep Frames"));

    Add(EBlockType::SetVar,           Variables, NSLOCTEXT("BlockEditor","N_SetVar",       "Set Var"));
    Add(EBlockType::GetVar,           Variables, NSLOCTEXT("BlockEditor","N_GetVar",       "Get Var"));
    Add(EBlockType::SetVarBool,       Variables, NSLOCTEXT("BlockEditor","N_SetBool",      "Set Var Bool"));
    Add(EBlockType::GetVarBool,       Variables, NSLOCTEXT("BlockEditor","N_GetBool",      "Get Var Bool"));
    Add(EBlockType::Compare,          Variables, NSLOCTEXT("BlockEditor","N_Cmp",          "Compare"));

    Add(EBlockType::InvokeTotem,      Invocation,NSLOCTEXT("BlockEditor","N_InvTot",       "Invoke Totem"));
    Add(EBlockType::InvokeSpell,      Invocation,NSLOCTEXT("BlockEditor","N_InvSpl",       "Invoke Spell"));

    Add(EBlockType::QueryNear,        Query,     NSLOCTEXT("BlockEditor","N_QNear",        "Query Near"));
    Add(EBlockType::QueryNearest,     Query,     NSLOCTEXT("BlockEditor","N_QNearest",     "Query Nearest"));
    Add(EBlockType::GetEntityProp,    Query,     NSLOCTEXT("BlockEditor","N_GetEnt",       "Get Entity Prop"));
    Add(EBlockType::SetEntityProp,    Query,     NSLOCTEXT("BlockEditor","N_SetEnt",       "Set Entity Prop"));
    Add(EBlockType::FocalPoint,       Query,     NSLOCTEXT("BlockEditor","N_Focal",        "Focal Point"));
    Add(EBlockType::Raycast,          Query,     NSLOCTEXT("BlockEditor","N_Ray",          "Raycast"));

    Add(EBlockType::RisingEdge,       Edge,      NSLOCTEXT("BlockEditor","N_REdge",        "Rising Edge"));
    Add(EBlockType::FallingEdge,      Edge,      NSLOCTEXT("BlockEditor","N_FEdge",        "Falling Edge"));
    Add(EBlockType::AlternateTrigger, Edge,      NSLOCTEXT("BlockEditor","N_Alt",          "Alternate"));
    Add(EBlockType::SinglePulse,      Edge,      NSLOCTEXT("BlockEditor","N_SPulse",       "Single Pulse"));

    Add(EBlockType::Broadcast,        Broadcast, NSLOCTEXT("BlockEditor","N_Bcast",        "Broadcast"));
    Add(EBlockType::BroadcastAndWait, Broadcast, NSLOCTEXT("BlockEditor","N_BcastW",       "Broadcast & Wait"));
    Add(EBlockType::OnReceive,        Broadcast, NSLOCTEXT("BlockEditor","N_OnRecv",       "On Receive"));

    Add(EBlockType::VecMake,          Vector,    NSLOCTEXT("BlockEditor","N_VMake",        "Vec Make"));
    Add(EBlockType::VecAdd,           Vector,    NSLOCTEXT("BlockEditor","N_VAdd",         "Vec Add"));
    Add(EBlockType::VecSub,           Vector,    NSLOCTEXT("BlockEditor","N_VSub",         "Vec Sub"));
    Add(EBlockType::VecScale,         Vector,    NSLOCTEXT("BlockEditor","N_VScale",       "Vec Scale"));
    Add(EBlockType::VecNorm,          Vector,    NSLOCTEXT("BlockEditor","N_VNorm",        "Vec Normalize"));
    Add(EBlockType::VecDot,           Vector,    NSLOCTEXT("BlockEditor","N_VDot",         "Vec Dot"));
    Add(EBlockType::VecLength,        Vector,    NSLOCTEXT("BlockEditor","N_VLen",         "Vec Length"));
    Add(EBlockType::VecFromEntity,    Vector,    NSLOCTEXT("BlockEditor","N_VFromEnt",     "Vec From Entity"));

    Add(EBlockType::Anchor,           Advanced,  NSLOCTEXT("BlockEditor","N_Anchor",       "Anchor Snapshot"));
    Add(EBlockType::Rollback,         Advanced,  NSLOCTEXT("BlockEditor","N_Rollback",     "Rollback"));
    Add(EBlockType::DamageShield,     Advanced,  NSLOCTEXT("BlockEditor","N_DmgShield",    "Damage Shield"));
    Add(EBlockType::DeathGuard,       Advanced,  NSLOCTEXT("BlockEditor","N_DeathG",       "Death Guard"));
    Add(EBlockType::SetActivationInstant,   Advanced, NSLOCTEXT("BlockEditor","N_ActInst", "Activation: Instant"));
    Add(EBlockType::SetActivationDeclare,   Advanced, NSLOCTEXT("BlockEditor","N_ActDecl", "Activation: Declare"));
    Add(EBlockType::SetActivationSustained, Advanced, NSLOCTEXT("BlockEditor","N_ActSust", "Activation: Sustained"));
}

const FPinConnectionResponse UBlockEdGraphSchema::CanCreateConnection(
    const UEdGraphPin* A, const UEdGraphPin* B) const
{
    if (!A || !B)
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NSLOCTEXT("BlockEditor","Err_NullPin","Invalid pin"));

    if (A->GetOwningNode() == B->GetOwningNode())
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NSLOCTEXT("BlockEditor","Err_Self","Cannot connect to self"));

    if (A->Direction == B->Direction)
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NSLOCTEXT("BlockEditor","Err_Dir","Cannot connect same-direction pins"));

    if (A->PinType.PinCategory != ExecCategory || B->PinType.PinCategory != ExecCategory)
        return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NSLOCTEXT("BlockEditor","Err_Type","Only execution pins can connect"));

    return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, FText::GetEmpty());
}

bool UBlockEdGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
    if (CanCreateConnection(A, B).Response == CONNECT_RESPONSE_DISALLOW)
        return false;

    // Each exec-input accepts only one incoming connection; break existing.
    UEdGraphPin* InPin = (A->Direction == EGPD_Input) ? A : B;
    if (InPin->LinkedTo.Num() > 0)
        BreakPinLinks(*InPin, false);

    A->MakeLinkTo(B);
    A->GetOwningNode()->GetGraph()->NotifyGraphChanged();
    return true;
}

FLinearColor UBlockEdGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
    return FLinearColor::White;
}

void UBlockEdGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
    for (UEdGraphPin* Pin : TargetNode.Pins)
        BreakPinLinks(*Pin, false);
    TargetNode.GetGraph()->NotifyGraphChanged();
}

void UBlockEdGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
    TArray<UEdGraphPin*> LinkedCopy = TargetPin.LinkedTo;
    for (UEdGraphPin* Other : LinkedCopy)
        Other->LinkedTo.Remove(&TargetPin);
    TargetPin.LinkedTo.Empty();

    if (bSendsNodeNotification)
        TargetPin.GetOwningNode()->GetGraph()->NotifyGraphChanged();
}

void UBlockEdGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
    if (!SourcePin || !TargetPin) return;
    SourcePin->LinkedTo.Remove(TargetPin);
    TargetPin->LinkedTo.Remove(SourcePin);
    SourcePin->GetOwningNode()->GetGraph()->NotifyGraphChanged();
}

void UBlockEdGraphSchema::GetGraphDisplayInformation(
    const UEdGraph& Graph, FGraphDisplayInfo& DisplayInfo) const
{
    DisplayInfo.DisplayName = NSLOCTEXT("BlockEditor","GraphTitle","Spell Block Graph");
    DisplayInfo.Tooltip     = NSLOCTEXT("BlockEditor","GraphTooltip","Design your spell using block nodes. Right-click to add blocks.");
}
