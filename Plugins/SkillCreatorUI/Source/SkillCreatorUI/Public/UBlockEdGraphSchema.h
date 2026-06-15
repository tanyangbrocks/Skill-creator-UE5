#pragma once
#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "BlockType.h"
#include "UBlockEdGraphSchema.generated.h"

// Action placed in the graph context menu: creates one UBlockEdGraphNode.
USTRUCT()
struct SKILLCREATORUI_API FBlockSchemaAction : public FEdGraphSchemaAction
{
    GENERATED_BODY()

    EBlockType BlockTypeToCreate = EBlockType::Wait;

    FBlockSchemaAction() : FEdGraphSchemaAction() {}
    FBlockSchemaAction(EBlockType InType,
                       FText MenuCategory, FText MenuDesc, FText Tooltip,
                       int32 Grouping = 0)
        : FEdGraphSchemaAction(MoveTemp(MenuCategory), MoveTemp(MenuDesc),
                               MoveTemp(Tooltip), Grouping)
        , BlockTypeToCreate(InType) {}

    // UE5.7 uses FVector2f for graph positions.
    virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph,
                                        UEdGraphPin* FromPin,
                                        const FVector2f& Location,
                                        bool bSelectNewNode = true) override;
    virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph,
                                        TArray<UEdGraphPin*>& FromPins,
                                        const FVector2f& Location,
                                        bool bSelectNewNode = true) override;
};

// Schema for the spell block graph.
// Defines valid connections (exec→exec only) and the context menu.
UCLASS()
class SKILLCREATORUI_API UBlockEdGraphSchema : public UEdGraphSchema
{
    GENERATED_BODY()
public:
    // Execution pin category name shared with pin creators.
    static const FName ExecCategory;

    // UEdGraphSchema overrides
    virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
    virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
    virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;
    virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
    virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const override;
    virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override;
    virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;
    virtual void GetGraphDisplayInformation(const UEdGraph& Graph, FGraphDisplayInfo& DisplayInfo) const override;
};
