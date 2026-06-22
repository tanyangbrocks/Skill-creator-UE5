#include "UBlockDropZoneWidget.h"
#include "UBlockDragDropOp.h"
#include "FBlockNode.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"

void UBlockDropZoneWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetHeightOverride(6.f);
    WidgetTree->RootWidget = Size;

    Line = WidgetTree->ConstructWidget<UBorder>();
    Line->SetBrushColor(FLinearColor(0.4f, 0.8f, 0.4f, 0.f)); // 對應 Godot _lineStyle 初始透明
    Size->SetContent(Line);
}

void UBlockDropZoneWidget::Setup(TArray<TUniquePtr<FBlockNode>>* InParentList, int32 InInsertAt,
                                  TFunction<void()> InOnChanged)
{
    ParentList = InParentList;
    InsertAt   = InInsertAt;
    OnChanged  = MoveTemp(InOnChanged);
}

void UBlockDropZoneWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
                                              UDragDropOperation* InOperation)
{
    if (Line && Cast<UBlockDragDropOp>(InOperation))
        Line->SetBrushColor(FLinearColor(0.4f, 0.8f, 0.4f, 1.f)); // 對應 Godot MouseEntered 高亮
}

void UBlockDropZoneWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (Line) Line->SetBrushColor(FLinearColor(0.4f, 0.8f, 0.4f, 0.f));
}

bool UBlockDropZoneWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
                                         UDragDropOperation* InOperation)
{
    if (Line) Line->SetBrushColor(FLinearColor(0.4f, 0.8f, 0.4f, 0.f));

    UBlockDragDropOp* Op = Cast<UBlockDragDropOp>(InOperation);
    if (!Op || !ParentList) return false;

    int32 ActualInsertAt = InsertAt;
    TUniquePtr<FBlockNode> MovedNode;

    if (!Op->bFromPalette && Op->SourceList)
    {
        // 搬移既有卡片（對應 Godot ScratchCanvas.cs:963-971）
        if (Op->SourceList == ParentList && Op->SourceIndex < ActualInsertAt) --ActualInsertAt;
        if (Op->SourceList == ParentList && ActualInsertAt == Op->SourceIndex)
            return true; // 放回原位，no-op

        MovedNode = MoveTemp((*Op->SourceList)[Op->SourceIndex]);
        Op->SourceList->RemoveAt(Op->SourceIndex);
    }
    else
    {
        // Palette 新建（對應 Godot AbilityEditorUI.cs:880-916 PaletteBlockDropped）
        MovedNode = Op->CreateBlockNode();
    }

    ParentList->Insert(MoveTemp(MovedNode), FMath::Clamp(ActualInsertAt, 0, ParentList->Num()));
    if (OnChanged) OnChanged();
    return true;
}
