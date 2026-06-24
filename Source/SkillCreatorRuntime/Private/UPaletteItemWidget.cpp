#include "UPaletteItemWidget.h"
#include "UBlockDragDropOp.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "SlateBrushHelpers.h"

void UPaletteItemWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    Bg = WidgetTree->ConstructWidget<UBorder>();
    Bg->SetPadding(FMargin(6.f, 3.f));
    WidgetTree->RootWidget = Bg;

    // Godot Btn() 文字（AbilityEditorUI.cs:630/817 等）本身不自動換行，但 Godot 的列高是
    // CustomMinimumSize=(0,30)/(0,26) 固定值，文字超寬只會被裁切，視覺上不明顯；UE5 的
    // UTextBlock 預設不換行則會直接溢出列外蓋住右側子標籤欄。開啟 AutoWrapText 讓較長的
    // 技能因子/刻印描述（含 LV 標籤、pt 數字）能在 220px 欄寬內完整換行顯示，不裁切文字。
    LabelText = WidgetTree->ConstructWidget<UTextBlock>();
    LabelText->SetAutoWrapText(true);
    Bg->SetContent(LabelText);
}

void UPaletteItemWidget::Setup(const FText& InLabel, const FLinearColor& InColor, bool bInLocked,
                                TFunction<void(UBlockDragDropOp&)> InConfigureDrag)
{
    ItemColor     = InColor;
    bIsLocked     = bInLocked;
    ConfigureDrag = MoveTemp(InConfigureDrag);

    if (!Bg || !LabelText) return;

    // 鎖等級時整體灰階顯示（對應 Godot AbilityEditorUI.cs:823 locked ? gray : EngraveClr(...)）
    const FLinearColor Display = bIsLocked ? FLinearColor(0.40f, 0.40f, 0.42f) : ItemColor;
    Bg->SetBrush(MakeSolidBrush(FLinearColor(Display.R * 0.35f, Display.G * 0.35f, Display.B * 0.35f, 1.f)));
    LabelText->SetColorAndOpacity(FSlateColor(Display));
    LabelText->SetText(InLabel);
}

FReply UPaletteItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsLocked) return FReply::Unhandled();
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
        return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UPaletteItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
                                               UDragDropOperation*& OutOperation)
{
    UBlockDragDropOp* Op = NewObject<UBlockDragDropOp>(this);
    Op->bFromPalette = true;
    if (ConfigureDrag) ConfigureDrag(*Op);

    // 拖拉中跟著游標的預覽（對應 Godot SetDragPreview(BuildDragPreview(_block))）：
    // 另建一個獨立小色塊+文字，不能重用 TakeWidget()——那會把本項目從 Palette 清單
    // 裡的原始 Slot 摘掉並重新掛到拖拉層，導致清單拖拉中出現一個洞。
    UBorder* Preview = WidgetTree->ConstructWidget<UBorder>();
    Preview->SetPadding(FMargin(6.f, 3.f));
    Preview->SetBrush(MakeSolidBrush(FLinearColor(ItemColor.R * 0.5f, ItemColor.G * 0.5f, ItemColor.B * 0.5f, 0.9f)));
    UTextBlock* PreviewText = WidgetTree->ConstructWidget<UTextBlock>();
    PreviewText->SetText(LabelText->GetText());
    PreviewText->SetColorAndOpacity(FSlateColor(ItemColor));
    Preview->SetContent(PreviewText);

    Op->DefaultDragVisual = Preview;
    Op->Pivot = EDragPivot::MouseDown;
    OutOperation = Op;
}
