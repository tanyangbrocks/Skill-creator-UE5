#include "UCardDragHandleWidget.h"
#include "UBlockDragDropOp.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"

void UCardDragHandleWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    Bg = WidgetTree->ConstructWidget<UBorder>();
    WidgetTree->RootWidget = Bg;
}

void UCardDragHandleWidget::Setup(const FLinearColor& InColor, TArray<TUniquePtr<FBlockNode>>* InParentList, int32 InIndex)
{
    HandleColor   = InColor;
    ParentList    = InParentList;
    IndexInParent = InIndex;
    if (Bg) Bg->SetBrushColor(HandleColor);
}

FReply UCardDragHandleWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
        return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UCardDragHandleWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
                                                  UDragDropOperation*& OutOperation)
{
    UBlockDragDropOp* Op = NewObject<UBlockDragDropOp>(this);
    Op->bFromPalette = false;
    Op->SourceList   = ParentList;
    Op->SourceIndex  = IndexInParent;

    UBorder* Preview = WidgetTree->ConstructWidget<UBorder>();
    Preview->SetBrushColor(FLinearColor(HandleColor.R, HandleColor.G, HandleColor.B, 0.6f));
    Op->DefaultDragVisual = Preview;
    Op->Pivot = EDragPivot::MouseDown;
    OutOperation = Op;
}
