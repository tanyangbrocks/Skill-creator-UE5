#pragma once
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "StructUtils/InstancedStruct.h"
#include "BlockType.h"
#include "UBlockEdGraphNode.generated.h"

// Editor-side graph node for one spell block.
// Lives in the UBlockEdGraph; serialized separately from FBlockNode (the runtime type).
// Conversion: UBlockEdGraph::ToBlockNodes() / FromBlockNodes()
UCLASS()
class SKILLCREATORUI_API UBlockEdGraphNode : public UEdGraphNode
{
    GENERATED_BODY()
public:
    // Block category: drives pin layout, title color, and serialization.
    UPROPERTY(EditAnywhere, Category="Block")
    EBlockType BlockType = EBlockType::Wait;

    // Editable parameters — same map layout as FBlockNode::Params.
    // Selecting this node in the Details panel shows spinboxes / dropdowns
    // auto-generated from each FInstancedStruct's UPROPERTY fields.
    UPROPERTY(EditAnywhere, Category="Block")
    TMap<FName, FInstancedStruct> NodeParams;

    // UEdGraphNode overrides
    virtual void AllocateDefaultPins() override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FLinearColor GetNodeTitleColor() const override;
};
