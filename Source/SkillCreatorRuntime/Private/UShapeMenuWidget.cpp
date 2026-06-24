#include "UShapeMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "SlateBrushHelpers.h"

static void PinCenterShape(UWidget* W, FVector2D HalfSize)
{
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(W->Slot))
    {
        S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
        S->SetPosition(-HalfSize);
        S->SetSize(HalfSize * 2.f);
        S->SetAlignment(FVector2D::ZeroVector);
    }
}

FString UShapeMenuWidget::ShapeName(EPlacementShape S)
{
    switch (S)
    {
        case EPlacementShape::Single:   return TEXT("單格");
        case EPlacementShape::Cube:     return TEXT("立方");
        case EPlacementShape::Sphere:   return TEXT("球形");
        case EPlacementShape::Cylinder: return TEXT("圓柱");
        case EPlacementShape::Flat:     return TEXT("平面");
        default:                        return TEXT("?");
    }
}

void UShapeMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // 攔截點擊
    UBorder* ClickBlock = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Root->AddChild(ClickBlock);
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(ClickBlock->Slot))
        S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    ClickBlock->SetBrush(MakeSolidBrush(FLinearColor(0.f, 0.f, 0.f, 0.2f)));

    // 中央面板 210×280
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
    Panel->SetBrush(MakeSolidBrush(FLinearColor(0.10f, 0.10f, 0.16f, 0.95f)));
    Panel->SetPadding(FMargin(14.f, 12.f));
    Root->AddChild(Panel);
    PinCenterShape(Panel, FVector2D(105.f, 140.f));

    UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Panel->AddChild(VBox);

    // 標題
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Title->SetText(FText::FromString(TEXT("形狀選擇（N 關閉）")));
    {
        FSlateFontInfo F = Title->GetFont();
        F.Size = 13;
        Title->SetFont(F);
    }
    Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.85f, 1.0f)));
    Title->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(Title))
    {
        VS->SetHorizontalAlignment(HAlign_Fill);
        VS->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    }

    // 2-column grid：5 shape buttons
    static const EPlacementShape Shapes[ShapeCount] = {
        EPlacementShape::Single, EPlacementShape::Cube,
        EPlacementShape::Sphere, EPlacementShape::Cylinder,
        EPlacementShape::Flat,
    };

    UGridPanel* Grid = WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass());
    if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(Grid))
        VS->SetHorizontalAlignment(HAlign_Fill);

    for (int32 i = 0; i < ShapeCount; ++i)
    {
        UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());

        UTextBlock* BtnTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        BtnTxt->SetText(FText::FromString(ShapeName(Shapes[i])));
        {
            FSlateFontInfo F = BtnTxt->GetFont();
            F.Size = 13;
            BtnTxt->SetFont(F);
        }
        BtnTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.88f, 0.95f)));
        BtnTxt->SetJustification(ETextJustify::Center);
        Btn->AddChild(BtnTxt);

        if (UGridSlot* GS = Grid->AddChildToGrid(Btn, i / 2, i % 2))
        {
            GS->SetPadding(FMargin(3.f));
            GS->SetHorizontalAlignment(HAlign_Fill);
        }
        ShapeButtons[i]     = Btn;
        ShapeButtonTexts[i] = BtnTxt;
    }

    ShapeButtons[0]->OnClicked.AddDynamic(this, &UShapeMenuWidget::OnShape0Clicked);
    ShapeButtons[1]->OnClicked.AddDynamic(this, &UShapeMenuWidget::OnShape1Clicked);
    ShapeButtons[2]->OnClicked.AddDynamic(this, &UShapeMenuWidget::OnShape2Clicked);
    ShapeButtons[3]->OnClicked.AddDynamic(this, &UShapeMenuWidget::OnShape3Clicked);
    ShapeButtons[4]->OnClicked.AddDynamic(this, &UShapeMenuWidget::OnShape4Clicked);

    // 目前形狀顯示
    CurrentShapeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    {
        FSlateFontInfo F = CurrentShapeLabel->GetFont();
        F.Size = 11;
        CurrentShapeLabel->SetFont(F);
    }
    CurrentShapeLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.60f, 0.75f, 0.60f)));
    CurrentShapeLabel->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(CurrentShapeLabel))
    {
        VS->SetHorizontalAlignment(HAlign_Fill);
        VS->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
    }

    RefreshButtonHighlight();
}

void UShapeMenuWidget::SetActiveShape(EPlacementShape Shape)
{
    ActiveShape = Shape;
    RefreshButtonHighlight();
}

void UShapeMenuWidget::SelectShape(EPlacementShape S)
{
    ActiveShape = S;
    RefreshButtonHighlight();
    OnShapeSelected.ExecuteIfBound(S);
    OnCloseRequested.ExecuteIfBound();
}

void UShapeMenuWidget::RefreshButtonHighlight()
{
    static const EPlacementShape Shapes[ShapeCount] = {
        EPlacementShape::Single, EPlacementShape::Cube,
        EPlacementShape::Sphere, EPlacementShape::Cylinder,
        EPlacementShape::Flat,
    };
    for (int32 i = 0; i < ShapeCount; ++i)
    {
        if (!ShapeButtonTexts[i]) continue;
        bool bActive = (Shapes[i] == ActiveShape);
        ShapeButtonTexts[i]->SetColorAndOpacity(FSlateColor(
            bActive ? FLinearColor(0.40f, 0.85f, 1.00f) : FLinearColor(0.85f, 0.88f, 0.95f)));
    }
    if (CurrentShapeLabel)
        CurrentShapeLabel->SetText(FText::FromString(
            FString::Printf(TEXT("目前：%s"), *ShapeName(ActiveShape))));
}

void UShapeMenuWidget::OnShape0Clicked() { SelectShape(EPlacementShape::Single);   }
void UShapeMenuWidget::OnShape1Clicked() { SelectShape(EPlacementShape::Cube);     }
void UShapeMenuWidget::OnShape2Clicked() { SelectShape(EPlacementShape::Sphere);   }
void UShapeMenuWidget::OnShape3Clicked() { SelectShape(EPlacementShape::Cylinder); }
void UShapeMenuWidget::OnShape4Clicked() { SelectShape(EPlacementShape::Flat);     }
void UShapeMenuWidget::OnCloseClicked()  { OnCloseRequested.ExecuteIfBound();      }
