#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "FBlockNode.h"

class UBlockEdGraph;
class SGraphEditor;

// Slate widget hosting the SGraphEditor for the spell block system.
// Usage:
//   UBlockEdGraph* Graph = NewObject<UBlockEdGraph>(...);
//   Graph->Schema = UBlockEdGraphSchema::StaticClass();
//   TSharedRef<SBlockEditorWidget> W = SNew(SBlockEditorWidget).GraphToEdit(Graph);
class SKILLCREATORUI_API SBlockEditorWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SBlockEditorWidget) : _GraphToEdit(nullptr) {}
        SLATE_ARGUMENT(UBlockEdGraph*, GraphToEdit)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // Returns the current block node tree (for SpellCompiler).
    TArray<TUniquePtr<FBlockNode>> GetBlockNodes() const;

    UBlockEdGraph* GetGraph() const { return Graph.Get(); }

    // 切換到另一個技能 group（切換 Graph 並刷新 SGraphEditor）
    void SwitchEditorGroup(int32 GroupIndex, UBlockEdGraph* NewGraph);

    // 每當 Graph 內容變更（節點增減 / 連線異動）時廣播
    FSimpleMulticastDelegate OnChanged;

private:
    TStrongObjectPtr<UBlockEdGraph> Graph;
    TSharedPtr<SGraphEditor> GraphEditor;

    void HandleGraphChanged(const struct FEdGraphEditAction&);
    void BindGraphChangedHandler();

    FDelegateHandle GraphChangedHandle;
};
