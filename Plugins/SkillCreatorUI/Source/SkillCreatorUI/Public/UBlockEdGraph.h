#pragma once
#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "FBlockNode.h"
#include "UBlockEdGraphNode.h"
#include "UBlockEdGraph.generated.h"

// Graph object holding UBlockEdGraphNodes.
// ToBlockNodes()   — serialize graph → runtime FBlockNode tree (for SpellCompiler)
// FromBlockNodes() — deserialize runtime tree → visual graph
UCLASS()
class SKILLCREATORUI_API UBlockEdGraph : public UEdGraph
{
    GENERATED_BODY()
public:
    // Convert the visual graph to the runtime FBlockNode tree.
    // Root nodes = nodes whose "In" pin has no incoming connections.
    // Sorted top-to-bottom (NodePosY) to preserve sequence order.
    TArray<TUniquePtr<FBlockNode>> ToBlockNodes() const;

    // Populate the graph from a runtime block list.
    // Clears any existing nodes and auto-lays them out.
    void FromBlockNodes(const TArray<TUniquePtr<FBlockNode>>& Blocks,
                        FVector2D StartPos = FVector2D(100.f, 100.f));

private:
    // Recursively convert one graph node and its connected children.
    // Visited set prevents infinite recursion on cycles (schema should block them, but be safe).
    TUniquePtr<FBlockNode> NodeToBlock(UBlockEdGraphNode* GraphNode,
                                        TSet<UBlockEdGraphNode*>& Visited) const;

    // Recursively place block list, returning the next available Y position.
    float PlaceBlockList(const TArray<TUniquePtr<FBlockNode>>& Blocks,
                         UBlockEdGraphNode* ParentNode, FName OutPinName,
                         FVector2D Pos, float ColumnOffsetX);
};
