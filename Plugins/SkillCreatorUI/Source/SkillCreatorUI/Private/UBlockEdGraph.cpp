#include "UBlockEdGraph.h"
#include "UBlockEdGraphSchema.h"
#include "EdGraph/EdGraphPin.h"

// ── ToBlockNodes ──────────────────────────────────────────────────────────────

TArray<TUniquePtr<FBlockNode>> UBlockEdGraph::ToBlockNodes() const
{
    // Root nodes: "In" pin has no incoming connections.
    TArray<UBlockEdGraphNode*> RootNodes;
    for (UEdGraphNode* Node : Nodes)
    {
        UBlockEdGraphNode* BN = Cast<UBlockEdGraphNode>(Node);
        if (!BN) continue;
        const UEdGraphPin* InPin = BN->FindPin(TEXT("In"), EGPD_Input);
        if (!InPin || InPin->LinkedTo.IsEmpty())
            RootNodes.Add(BN);
    }

    // Preserve sequence by visual Y position.
    RootNodes.Sort([](const UBlockEdGraphNode& A, const UBlockEdGraphNode& B){
        return A.NodePosY < B.NodePosY;
    });

    TArray<TUniquePtr<FBlockNode>> Result;
    Result.Reserve(RootNodes.Num());
    TSet<UBlockEdGraphNode*> Visited;
    for (UBlockEdGraphNode* Root : RootNodes)
        if (TUniquePtr<FBlockNode> B = NodeToBlock(Root, Visited))
            Result.Add(MoveTemp(B));
    return Result;
}

TUniquePtr<FBlockNode> UBlockEdGraph::NodeToBlock(UBlockEdGraphNode* GN,
                                                    TSet<UBlockEdGraphNode*>& Visited) const
{
    if (!GN) return nullptr;
    if (Visited.Contains(GN)) return nullptr;  // 循環保護
    Visited.Add(GN);

    auto Block = MakeUnique<FBlockNode>();
    Block->Type   = GN->BlockType;
    Block->Params = GN->NodeParams;

    auto FollowPin = [&](const FName PinName, TArray<TUniquePtr<FBlockNode>>& OutBranch)
    {
        const UEdGraphPin* Pin = GN->FindPin(PinName, EGPD_Output);
        if (!Pin) return;

        // Collect + sort children by Y so order is deterministic.
        TArray<UBlockEdGraphNode*> Children;
        for (UEdGraphPin* LP : Pin->LinkedTo)
            if (UBlockEdGraphNode* Child = Cast<UBlockEdGraphNode>(LP->GetOwningNode()))
                Children.Add(Child);
        Children.Sort([](const UBlockEdGraphNode& A, const UBlockEdGraphNode& B){
            return A.NodePosY < B.NodePosY;
        });
        for (UBlockEdGraphNode* Child : Children)
            if (TUniquePtr<FBlockNode> CB = NodeToBlock(Child, Visited))
                OutBranch.Add(MoveTemp(CB));
    };

    FollowPin(TEXT("Then"), Block->ThenBranch);
    FollowPin(TEXT("Else"), Block->ElseBranch);
    FollowPin(TEXT("Body"), Block->LoopBody);
    return Block;
}

// ── FromBlockNodes ────────────────────────────────────────────────────────────

void UBlockEdGraph::FromBlockNodes(const TArray<TUniquePtr<FBlockNode>>& Blocks,
                                    FVector2D StartPos)
{
    // Destroy existing nodes.
    TArray<UEdGraphNode*> OldNodes = Nodes;
    for (UEdGraphNode* N : OldNodes) N->DestroyNode();

    PlaceBlockList(Blocks, nullptr, TEXT("Then"), StartPos, 0.f);
}

float UBlockEdGraph::PlaceBlockList(const TArray<TUniquePtr<FBlockNode>>& Blocks,
                                     UBlockEdGraphNode* ParentNode, FName OutPinName,
                                     FVector2D Pos, float ColumnOffsetX)
{
    constexpr float NodeW     = 200.f;
    constexpr float NodeH     = 80.f;
    constexpr float PadY      = 20.f;
    constexpr float BranchOff = 240.f;

    float CurY = Pos.Y;
    for (const TUniquePtr<FBlockNode>& Block : Blocks)
    {
        if (!Block) continue;

        UBlockEdGraphNode* Node = NewObject<UBlockEdGraphNode>(
            this, UBlockEdGraphNode::StaticClass(), NAME_None, RF_Transactional);
        Node->BlockType  = Block->Type;
        Node->NodeParams = Block->Params;
        Node->NodePosX   = static_cast<int32>(Pos.X + ColumnOffsetX);
        Node->NodePosY   = static_cast<int32>(CurY);
        Node->AllocateDefaultPins();
        AddNode(Node, false, false);

        // Wire to parent's output pin.
        if (ParentNode)
        {
            UEdGraphPin* OutPin = ParentNode->FindPin(OutPinName, EGPD_Output);
            UEdGraphPin* InPin  = Node->FindPin(TEXT("In"), EGPD_Input);
            if (OutPin && InPin)
                GetSchema()->TryCreateConnection(OutPin, InPin);
        }

        float NextY = CurY + NodeH + PadY;

        // Recurse into branches.
        if (!Block->ThenBranch.IsEmpty())
            NextY = PlaceBlockList(Block->ThenBranch, Node, TEXT("Then"),
                                   FVector2D(Pos.X + ColumnOffsetX + BranchOff, CurY), 0.f);
        if (!Block->ElseBranch.IsEmpty())
            NextY = FMath::Max(NextY,
                               PlaceBlockList(Block->ElseBranch, Node, TEXT("Else"),
                                              FVector2D(Pos.X + ColumnOffsetX + BranchOff * 2.f, CurY), 0.f));
        if (!Block->LoopBody.IsEmpty())
            NextY = FMath::Max(NextY,
                               PlaceBlockList(Block->LoopBody, Node, TEXT("Body"),
                                              FVector2D(Pos.X + ColumnOffsetX + BranchOff, CurY), 0.f));

        CurY = NextY;

        // After the first item ParentNode only connects to the first child.
        ParentNode = nullptr;
    }
    return CurY;
}
