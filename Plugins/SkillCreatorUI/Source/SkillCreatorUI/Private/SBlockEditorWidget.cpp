#include "SBlockEditorWidget.h"
#include "UBlockEdGraph.h"
#include "UBlockEdGraphSchema.h"
#include "GraphEditor.h"
#include "EdGraph/EdGraph.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "SpellArray.h"
#include "SpellSlotSync.h"

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
    PlayerLevel = InArgs._PlayerLevel;
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

// 對應 Godot SaveSpell()，C:\skill-creator\Scripts\UI\AbilityEditorUI.cs line 1276-1345。
// ① SyncSlotsFromBlocks（在記憶體建的暫時 FSpellArray 上跑，不需要真正的 FSpellArray 存在）
// ② 5 項驗證，全部收集後一次回報
// ③ 驗證失敗 → 彈出列出全部錯誤的 modal 視窗，不呼叫 OnSaveSpell（= 不存檔）
// ④ 驗證通過 → 呼叫 OnSaveSpell delegate，由 PlayerController 端真正寫入 Loadout
void SBlockEditorWidget::HandleSaveClicked()
{
    const FString Name = GetSpellName();
    TArray<TUniquePtr<FBlockNode>> Nodes = GetBlockNodes();

    // 暫時的 FSpellArray，只用來跑 SyncSlotsFromBlocks + 驗證；不持久化。
    FSpellArray TempSpell;
    TempSpell.Name = Name;
    TempSpell.SetBlocks(MoveTemp(Nodes));

    FSpellSlotSync::SyncSlotsFromBlocks(TempSpell);

    const TArray<FString> Errors =
        FSpellSlotSync::ValidateSpell(TempSpell, *TempSpell.Blocks, PlayerLevel);

    if (!Errors.IsEmpty())
    {
        if (StatusLabel.IsValid())
            StatusLabel->SetText(NSLOCTEXT("BlockEditor", "SaveFailed", "⚠ 儲存失敗，請見錯誤視窗"));
        ShowValidationErrors(Errors);
        return;
    }

    if (StatusLabel.IsValid())
    {
        StatusLabel->SetText(FText::FromString(
            FString::Printf(TEXT("✓ 槽位 %d「%s」已存"), ActiveSlot + 1, *Name)));
    }
    OnSaveSpell.ExecuteIfBound(Name, ActiveSlot);
}

// 對應 Godot SaveSpell() 失敗分支的 AcceptDialog（line 1305-1318）：
// Title="⚠ 儲存失敗"，DialogText = 所有錯誤訊息以換行串接，使用者確認/關閉後銷毀視窗。
void SBlockEditorWidget::ShowValidationErrors(const TArray<FString>& Errors)
{
    TSharedPtr<SWindow> ErrorWindow;

    TSharedRef<SScrollBox> ErrorList = SNew(SScrollBox);
    for (const FString& Err : Errors)
    {
        ErrorList->AddSlot()
        .Padding(4.f, 2.f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(FString::Printf(TEXT("• %s"), *Err)))
            .ColorAndOpacity(FLinearColor(1.f, 0.6f, 0.5f))
            .AutoWrapText(true)
        ];
    }

    ErrorWindow = SNew(SWindow)
        .Title(NSLOCTEXT("BlockEditor", "SaveFailedTitle", "⚠ 儲存失敗"))
        .ClientSize(FVector2D(420.f, 260.f))
        .SizingRule(ESizingRule::FixedSize)
        .SupportsMaximize(false)
        .SupportsMinimize(false)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(1.f).Padding(8.f)
            [
                ErrorList
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(8.f)
            [
                SNew(SButton)
                .Text(NSLOCTEXT("BlockEditor", "SaveFailedOk", "確認"))
                .OnClicked_Lambda([ErrorWindowWeak = TWeakPtr<SWindow>(ErrorWindow)]() -> FReply
                {
                    if (TSharedPtr<SWindow> Win = ErrorWindowWeak.Pin())
                        Win->RequestDestroyWindow();
                    return FReply::Handled();
                })
            ]
        ];

    FSlateApplication::Get().AddModalWindow(ErrorWindow.ToSharedRef(), SharedThis(this));
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
