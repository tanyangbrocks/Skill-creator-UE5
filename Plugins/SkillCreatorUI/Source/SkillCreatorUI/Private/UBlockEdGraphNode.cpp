#include "UBlockEdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "UBlockEdGraphSchema.h"

void UBlockEdGraphNode::AllocateDefaultPins()
{
    // Execution input — always present.
    CreatePin(EGPD_Input,  UBlockEdGraphSchema::ExecCategory, TEXT("In"));

    // Primary sequential output — every block has one.
    CreatePin(EGPD_Output, UBlockEdGraphSchema::ExecCategory, TEXT("Then"));

    // Conditional branches.
    if (BlockType == EBlockType::If || BlockType == EBlockType::Evaluate)
        CreatePin(EGPD_Output, UBlockEdGraphSchema::ExecCategory, TEXT("Else"));

    // Loop body.
    if (BlockType == EBlockType::RepeatN        ||
        BlockType == EBlockType::RepeatWhile    ||
        BlockType == EBlockType::ForEachNearby)
        CreatePin(EGPD_Output, UBlockEdGraphSchema::ExecCategory, TEXT("Body"));
}

FText UBlockEdGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    // Use the UENUM display name (e.g. "Wait", "If", "RepeatN").
    const UEnum* E = StaticEnum<EBlockType>();
    if (!E) return FText::FromString(TEXT("Block"));
    const FString Name = E->GetNameStringByValue(static_cast<int64>(BlockType));
    return FText::FromString(Name);
}

FText UBlockEdGraphNode::GetTooltipText() const
{
    return GetNodeTitle(ENodeTitleType::FullTitle);
}

FLinearColor UBlockEdGraphNode::GetNodeTitleColor() const
{
    switch (BlockType)
    {
    // Control flow — orange
    case EBlockType::If:
    case EBlockType::Evaluate:
    case EBlockType::RepeatN:
    case EBlockType::RepeatWhile:
    case EBlockType::ForEachNearby:
    case EBlockType::RandomChoice:
    case EBlockType::SequentialGate:
        return FLinearColor(0.9f, 0.5f, 0.1f);

    // Edge triggers — yellow
    case EBlockType::RisingEdge:
    case EBlockType::FallingEdge:
    case EBlockType::AlternateTrigger:
    case EBlockType::SinglePulse:
        return FLinearColor(0.9f, 0.85f, 0.1f);

    // Invocations — blue
    case EBlockType::InvokeSpell:
    case EBlockType::InvokeTotem:
        return FLinearColor(0.1f, 0.5f, 0.9f);

    // Variables — green
    case EBlockType::SetVar:
    case EBlockType::GetVar:
    case EBlockType::SetVarBool:
    case EBlockType::GetVarBool:
    case EBlockType::Compare:
        return FLinearColor(0.1f, 0.8f, 0.3f);

    // Broadcasts — purple
    case EBlockType::Broadcast:
    case EBlockType::BroadcastAndWait:
    case EBlockType::OnReceive:
        return FLinearColor(0.6f, 0.2f, 0.8f);

    default:
        return FLinearColor(0.5f, 0.5f, 0.55f);
    }
}
