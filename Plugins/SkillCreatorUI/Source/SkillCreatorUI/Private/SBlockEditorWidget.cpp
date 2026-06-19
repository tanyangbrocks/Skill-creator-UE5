#include "SBlockEditorWidget.h"
#include "UBlockEdGraph.h"
#include "UBlockEdGraphSchema.h"
#include "GraphEditor.h"
#include "EdGraph/EdGraph.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"

void SBlockEditorWidget::HandleGraphChanged(const FEdGraphEditAction&)
{
    OnChanged.Broadcast();
}

void SBlockEditorWidget::BindGraphChangedHandler()
{
    if (!Graph) return;
    if (GraphChangedHandle.IsValid())
        Graph->RemoveOnGraphChangedHandler(GraphChangedHandle);
    GraphChangedHandle = Graph->AddOnGraphChangedHandler(
        FOnGraphChanged::FDelegate::CreateSP(this, &SBlockEditorWidget::HandleGraphChanged));
}

void SBlockEditorWidget::Construct(const FArguments& InArgs)
{
    Graph = TStrongObjectPtr<UBlockEdGraph>(InArgs._GraphToEdit);
    if (Graph && !Graph->Schema)
        Graph->Schema = UBlockEdGraphSchema::StaticClass();

    BindGraphChangedHandler();

    SGraphEditor::FGraphEditorEvents GraphEvents;

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
            .AutoHeight()
            .Padding(6.f, 4.f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
                [
                    SNew(STextBlock)
                    .Text(NSLOCTEXT("BlockEditor","NameLabel","技能整構名稱："))
                    .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
                ]
                + SHorizontalBox::Slot().FillWidth(1.f)
                [
                    SAssignNew(SpellNameBox, SEditableTextBox)
                    .HintText(NSLOCTEXT("BlockEditor","NameHint","輸入技能整構名稱"))
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
                [
                    SNew(SButton)
                    .Text(NSLOCTEXT("BlockEditor","SaveBtn","儲存技能整構"))
                    .ButtonColorAndOpacity(FLinearColor(0.15f, 0.35f, 0.55f))
                    .OnClicked_Lambda([this]() -> FReply { HandleSaveClicked(); return FReply::Handled(); })
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
                [
                    SAssignNew(StatusLabel, STextBlock)
                    .ColorAndOpacity(FLinearColor(0.5f, 0.9f, 0.5f))
                ]
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.f)
            [
                SAssignNew(GraphEditorContainer, SBox)
                [
                    SAssignNew(GraphEditor, SGraphEditor)
                    .AdditionalCommands(TSharedPtr<FUICommandList>())
                    .GraphToEdit(Graph.Get())
                    .GraphEvents(GraphEvents)
                    .IsEditable(true)
                    .ShowGraphStateOverlay(false)
                ]
            ]
        ]
    ];
}

TArray<TUniquePtr<FBlockNode>> SBlockEditorWidget::GetBlockNodes() const
{
    if (!Graph) return {};
    return Graph->ToBlockNodes();
}

FString SBlockEditorWidget::GetSpellName() const
{
    return SpellNameBox.IsValid() ? SpellNameBox->GetText().ToString() : FString();
}

void SBlockEditorWidget::SetSpellName(const FString& Name)
{
    if (SpellNameBox.IsValid())
        SpellNameBox->SetText(FText::FromString(Name));
}

void SBlockEditorWidget::HandleSaveClicked()
{
    const FString Name = GetSpellName();
    if (StatusLabel.IsValid())
    {
        StatusLabel->SetText(Name.IsEmpty()
            ? NSLOCTEXT("BlockEditor", "SaveNeedsName", "⚠ 請填寫技能整構名稱")
            : FText::FromString(FString::Printf(TEXT("✓ 槽位 %d「%s」已存"), ActiveSlot + 1, *Name)));
    }
    if (Name.IsEmpty()) return;
    OnSaveSpell.ExecuteIfBound(Name, ActiveSlot);
}

void SBlockEditorWidget::RebuildGraphEditor()
{
    if (!GraphEditorContainer.IsValid()) return;
    SGraphEditor::FGraphEditorEvents GraphEvents;
    TSharedRef<SGraphEditor> NewEditor =
        SNew(SGraphEditor)
        .AdditionalCommands(TSharedPtr<FUICommandList>())
        .GraphToEdit(Graph.Get())
        .GraphEvents(GraphEvents)
        .IsEditable(true)
        .ShowGraphStateOverlay(false);
    GraphEditor = NewEditor;
    GraphEditorContainer->SetContent(NewEditor);
}

void SBlockEditorWidget::SwitchEditorGroup(int32 /*GroupIndex*/, UBlockEdGraph* NewGraph)
{
    Graph = TStrongObjectPtr<UBlockEdGraph>(NewGraph);
    if (Graph && !Graph->Schema)
        Graph->Schema = UBlockEdGraphSchema::StaticClass();

    BindGraphChangedHandler();
    RebuildGraphEditor();
    OnChanged.Broadcast();
}
