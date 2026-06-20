#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UBlockCardWidget.generated.h"

class UVerticalBox;
class UBorder;
struct FBlockNode;

// 一張積木卡片（對應 Godot ScratchCanvas.BuildBlockCard，ScratchCanvas.cs:100-194）。
// UBlockEditorWidget 不持有任何「視覺節點」中介物件——卡片直接指向正在編輯的
// FBlockNode 樹本身，刪除/搬移都是直接操作 TUniquePtr<FBlockNode> 陣列，
// 結構變動後呼叫 OnChanged() 由上層整個 RebuildList()。
UCLASS()
class SKILLCREATORRUNTIME_API UBlockCardWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // OnDoubleClicked（Phase 6）：對應 Godot ScratchCanvas.BlockDoubleClicked，雙擊刻印
    // 積木卡片時通知上層（判斷是否為容器類 Action 刻印、要不要進入巢狀編輯）。
    void Setup(FBlockNode* InBlock, TArray<TUniquePtr<FBlockNode>>* InParentList, int32 InIndex,
               int32 InIndent, TFunction<void()> InOnChanged,
               TFunction<void(FBlockNode*)> InOnDoubleClicked = nullptr);

    // 把 Blocks 的卡片清單（含前後 DropZone）建進 Container；遞迴用於巢狀分支，
    // 跟根清單共用同一份邏輯（對應 Godot ScratchCanvas 的清單渲染對任意 List<BlockNode> 通用）。
    static void BuildBlockList(UWidget* WidgetContext, UVerticalBox* Container,
                                TArray<TUniquePtr<FBlockNode>>& Blocks, int32 Indent,
                                const TFunction<void()>& OnChanged,
                                const TFunction<void(FBlockNode*)>& OnDoubleClicked = nullptr);

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    FBlockNode* Block = nullptr;
    TArray<TUniquePtr<FBlockNode>>* ParentList = nullptr;
    int32 IndexInParent = -1;
    int32 Indent = 0;
    TFunction<void()> OnChanged;
    TFunction<void(FBlockNode*)> OnDoubleClicked;

    UFUNCTION() void OnDeleteClicked();

    // 「＋ 加入積木」按鈕用：FOnButtonClickedEvent 是 dynamic multicast，不支援 AddLambda，
    // 一張卡片最多 2 個巢狀分支（Then+Else，對應 If/RandomChoice/AlternateTrigger），
    // 固定兩個 UFUNCTION 處理常式 + 對應目標清單指標即可，不需要額外小 widget 類別。
    TArray<TUniquePtr<FBlockNode>>* AddBranchTarget[2] = { nullptr, nullptr };
    UFUNCTION() void OnAddToBranch0Clicked();
    UFUNCTION() void OnAddToBranch1Clicked();
    void AddDefaultBlockTo(TArray<TUniquePtr<FBlockNode>>* Target);

    void BuildCardRow(UVerticalBox* Outer);
    void BuildNestedBranches(UVerticalBox* Outer);
    UWidget* BuildBranchPanel(const FString& Label, TArray<TUniquePtr<FBlockNode>>& BranchBlocks, int32 SlotIndex);
};
