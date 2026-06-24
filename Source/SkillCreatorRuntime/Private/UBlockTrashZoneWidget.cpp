#include "UBlockTrashZoneWidget.h"
#include "UBlockDragDropOp.h"
#include "FBlockNode.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "SlateBrushHelpers.h"

// 對應 Godot ScriptCanvas.cs:107-115 _trashStyle 三段色（一般/拖曳中/懸停在垃圾桶上）
static const FLinearColor TrashIdleColor   (0.22f, 0.06f, 0.06f, 0.65f);
static const FLinearColor TrashHoverColor  (0.80f, 0.12f, 0.12f, 0.95f);

void UBlockTrashZoneWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // 48×48，對應 Godot ScriptCanvas.cs:126-133（OffsetLeft/Top=-56, OffsetRight/Bottom=-8
    // → 48px 寬高，錨點畫布右下角）
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(48.f);
    Size->SetHeightOverride(48.f);
    WidgetTree->RootWidget = Size;

    Bg = WidgetTree->ConstructWidget<UBorder>();
    Bg->SetBrush(MakeSolidBrush(TrashIdleColor));
    Size->SetContent(Bg);

    Icon = WidgetTree->ConstructWidget<UTextBlock>();
    Icon->SetText(FText::FromString(TEXT("\U0001F5D1"))); // 🗑，對應 Godot ScriptCanvas.cs:116-124
    Icon->SetJustification(ETextJustify::Center);
    {
        FSlateFontInfo F = Icon->GetFont();
        F.Size = 20;
        Icon->SetFont(F);
    }
    Bg->SetContent(Icon);
}

void UBlockTrashZoneWidget::Setup(TFunction<void()> InOnChanged)
{
    OnChanged = MoveTemp(InOnChanged);
}

void UBlockTrashZoneWidget::SetHighlighted(bool bHighlighted)
{
    if (Bg) Bg->SetBrush(MakeSolidBrush(bHighlighted ? TrashHoverColor : TrashIdleColor));
}

void UBlockTrashZoneWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
                                               UDragDropOperation* InOperation)
{
    if (Cast<UBlockDragDropOp>(InOperation)) SetHighlighted(true);
}

void UBlockTrashZoneWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    SetHighlighted(false);
}

bool UBlockTrashZoneWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
                                          UDragDropOperation* InOperation)
{
    SetHighlighted(false);

    UBlockDragDropOp* Op = Cast<UBlockDragDropOp>(InOperation);
    if (!Op) return false;

    // 對應 Godot ScriptCanvas.cs:495-504 FinishDrag：拖到垃圾桶 → 刪除整串積木（不插入任何清單）。
    // 既有卡片搬移（bFromPalette=false）才需要真正從來源清單移除；調色盤新建型拖放
    // （bFromPalette=true）本來就還沒插入任何清單，這裡單純吃下這次拖放、不做事即等同取消新增。
    if (!Op->bFromPalette && Op->SourceList && Op->SourceList->IsValidIndex(Op->SourceIndex))
    {
        Op->SourceList->RemoveAt(Op->SourceIndex);
        if (OnChanged) OnChanged();
    }
    return true;
}
