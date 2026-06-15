#include "SBlockEditorWidget.h"
#include "UBlockEdGraph.h"
#include "UBlockEdGraphSchema.h"
#include "GraphEditor.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"

void SBlockEditorWidget::Construct(const FArguments& InArgs)
{
    Graph = TStrongObjectPtr<UBlockEdGraph>(InArgs._GraphToEdit);
    if (Graph && !Graph->Schema)
        Graph->Schema = UBlockEdGraphSchema::StaticClass();

    SGraphEditor::FGraphEditorEvents GraphEvents;
    // Additional graph events (node selection, etc.) can be bound here.

    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f))
        .Padding(0.f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.f, 4.f)
            [
                SNew(STextBlock)
                .Text(NSLOCTEXT("BlockEditor","Title","Spell Block Editor  —  Right-click canvas to add blocks"))
                .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.f)
            [
                SAssignNew(GraphEditor, SGraphEditor)
                .AdditionalCommands(TSharedPtr<FUICommandList>())
                .GraphToEdit(Graph.Get())
                .GraphEvents(GraphEvents)
                .IsEditable(true)
                .ShowGraphStateOverlay(false)
            ]
        ]
    ];
}

TArray<TUniquePtr<FBlockNode>> SBlockEditorWidget::GetBlockNodes() const
{
    if (!Graph) return {};
    return Graph->ToBlockNodes();
}
