#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "FBlockNode.h"

class UBlockEdGraph;
class SGraphEditor;
class SBox;
class SEditableTextBox;
class STextBlock;

// Slate widget hosting the SGraphEditor for the spell block system.
// Usage:
//   UBlockEdGraph* Graph = NewObject<UBlockEdGraph>(...);
//   Graph->Schema = UBlockEdGraphSchema::StaticClass();
//   TSharedRef<SBlockEditorWidget> W = SNew(SBlockEditorWidget).GraphToEdit(Graph);
class SKILLCREATORUI_API SBlockEditorWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SBlockEditorWidget) : _GraphToEdit(nullptr), _PlayerLevel(1) {}
        SLATE_ARGUMENT(UBlockEdGraph*, GraphToEdit)
        SLATE_ARGUMENT(int32, PlayerLevel)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // Returns the current block node tree (for SpellCompiler).
    TArray<TUniquePtr<FBlockNode>> GetBlockNodes() const;

    UBlockEdGraph* GetGraph() const { return Graph.Get(); }

    // 切換到另一個技能 group（切換 Graph 並刷新 SGraphEditor）
    void SwitchEditorGroup(int32 GroupIndex, UBlockEdGraph* NewGraph);

    // 每當 Graph 內容變更（節點增減 / 連線異動）時廣播
    FSimpleMulticastDelegate OnChanged;

    // 「儲存技能整構」按鈕被按下、且 5 項驗證全部通過後才廣播：技能名稱 + 目前編輯中的槽位索引
    DECLARE_DELEGATE_TwoParams(FOnSaveSpell, const FString& /*SpellName*/, int32 /*SlotIndex*/)
    FOnSaveSpell OnSaveSpell;

    FString GetSpellName() const;
    void    SetSpellName(const FString& Name);
    void    SetActiveSlot(int32 SlotIdx) { ActiveSlot = SlotIdx; }
    int32   GetActiveSlot() const { return ActiveSlot; }
    void    SetPlayerLevel(int32 InLevel) { PlayerLevel = InLevel; }

private:
    TStrongObjectPtr<UBlockEdGraph> Graph;
    TSharedPtr<SGraphEditor> GraphEditor;
    TSharedPtr<SBox>         GraphEditorContainer;
    TSharedPtr<SEditableTextBox> SpellNameBox;
    TSharedPtr<STextBlock>       StatusLabel;
    int32                        ActiveSlot   = 0;
    int32                        PlayerLevel  = 1;

    void HandleGraphChanged(const struct FEdGraphEditAction&);
    void BindGraphChangedHandler();
    void RebuildGraphEditor();
    void HandleSaveClicked();

    // 對應 Godot SaveSpell() 失敗時彈出的 AcceptDialog（line 1305-1318）：
    // 列出全部驗證錯誤訊息，使用者按確認/關閉後銷毀視窗，不存檔。
    void ShowValidationErrors(const TArray<FString>& Errors);

    FDelegateHandle GraphChangedHandle;
};
