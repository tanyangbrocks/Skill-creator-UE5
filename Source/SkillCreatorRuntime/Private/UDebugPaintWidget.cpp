#include "UDebugPaintWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "MaterialRegistry.h"

// Godot Main.cs:560-580：右上角面板，固定寬度，貼齊右側
static void PinTopRight(UWidget* W, FVector2D Size, FVector2D Offset)
{
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(W->Slot))
    {
        S->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
        S->SetPosition(Offset);
        S->SetSize(Size);
        S->SetAlignment(FVector2D::ZeroVector);
    }
}

FString UDebugPaintWidget::MatName(EMaterialType Mat)
{
    switch (Mat)
    {
        case EMaterialType::Sand:  return TEXT("沙");
        case EMaterialType::Water: return TEXT("水");
        case EMaterialType::Stone: return TEXT("石");
        case EMaterialType::Wood:  return TEXT("木");
        case EMaterialType::Fire:  return TEXT("火");
        case EMaterialType::Lava:  return TEXT("岩漿");
        default:                   return TEXT("?");
    }
}

void UDebugPaintWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
    Panel->SetBrushColor(FLinearColor(0.10f, 0.10f, 0.14f, 0.95f));
    Panel->SetPadding(FMargin(6.f));
    Root->AddChild(Panel);
    PinTopRight(Panel, FVector2D(186.f, 230.f), FVector2D(-200.f, 185.f));  // 對應 Godot Main.cs:561

    UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Panel->AddChild(VBox);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Title->SetText(FText::FromString(TEXT("材質選擇（左鍵繪製 / 右鍵清除）")));
    {
        FSlateFontInfo F = Title->GetFont();
        F.Size = 11;
        Title->SetFont(F);
    }
    Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.78f, 0.88f)));
    Title->SetAutoWrapText(true);
    if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(Title))
        VS->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

    // ── 材質按鈕（對應 Godot Main.cs:569-583）────────────────────────
    static const EMaterialType Mats[MatCount] = {
        EMaterialType::Sand, EMaterialType::Water, EMaterialType::Stone,
        EMaterialType::Wood, EMaterialType::Fire,  EMaterialType::Lava,
    };

    for (int32 i = 0; i < MatCount; ++i)
    {
        UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        const FLinearColor C = FMaterialRegistry::GetColor(Mats[i], 128);
        Btn->SetBackgroundColor(FLinearColor(C.R * 0.6f, C.G * 0.6f, C.B * 0.6f, 1.f));

        UTextBlock* BtnTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        BtnTxt->SetText(FText::FromString(MatName(Mats[i])));
        BtnTxt->SetJustification(ETextJustify::Center);
        Btn->AddChild(BtnTxt);

        if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(Btn))
            VS->SetPadding(FMargin(0.f, 1.f));

        MatButtons[i]     = Btn;
        MatButtonTexts[i] = BtnTxt;
    }

    MatButtons[0]->OnClicked.AddDynamic(this, &UDebugPaintWidget::OnMat0Clicked);
    MatButtons[1]->OnClicked.AddDynamic(this, &UDebugPaintWidget::OnMat1Clicked);
    MatButtons[2]->OnClicked.AddDynamic(this, &UDebugPaintWidget::OnMat2Clicked);
    MatButtons[3]->OnClicked.AddDynamic(this, &UDebugPaintWidget::OnMat3Clicked);
    MatButtons[4]->OnClicked.AddDynamic(this, &UDebugPaintWidget::OnMat4Clicked);
    MatButtons[5]->OnClicked.AddDynamic(this, &UDebugPaintWidget::OnMat5Clicked);

    UTextBlock* BrushTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    BrushTitle->SetText(FText::FromString(TEXT("筆刷大小")));
    {
        FSlateFontInfo F = BrushTitle->GetFont();
        F.Size = 11;
        BrushTitle->SetFont(F);
    }
    BrushTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.78f, 0.88f)));
    if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(BrushTitle))
        VS->SetPadding(FMargin(0.f, 6.f, 0.f, 2.f));

    // ── 筆刷大小按鈕（對應 Godot Main.cs:588-598："1"=0, "3"=1, "5"=2, "9"=4）─
    UHorizontalBox* BrushRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(BrushRow))
        VS->SetHorizontalAlignment(HAlign_Fill);

    static const TPair<FString, int32> Brushes[BrushCount] = {
        { TEXT("1"), 0 }, { TEXT("3"), 1 }, { TEXT("5"), 2 }, { TEXT("9"), 4 },
    };

    for (int32 i = 0; i < BrushCount; ++i)
    {
        UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        Btn->SetBackgroundColor(FLinearColor(0.22f, 0.22f, 0.28f, 1.f));

        UTextBlock* BtnTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        BtnTxt->SetText(FText::FromString(Brushes[i].Key));
        BtnTxt->SetJustification(ETextJustify::Center);
        Btn->AddChild(BtnTxt);

        if (UHorizontalBoxSlot* HS = BrushRow->AddChildToHorizontalBox(Btn))
        {
            HS->SetPadding(FMargin(1.f));
            HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }

        BrushButtons[i]     = Btn;
        BrushButtonTexts[i] = BtnTxt;
    }

    BrushButtons[0]->OnClicked.AddDynamic(this, &UDebugPaintWidget::OnBrush0Clicked);
    BrushButtons[1]->OnClicked.AddDynamic(this, &UDebugPaintWidget::OnBrush1Clicked);
    BrushButtons[2]->OnClicked.AddDynamic(this, &UDebugPaintWidget::OnBrush2Clicked);
    BrushButtons[3]->OnClicked.AddDynamic(this, &UDebugPaintWidget::OnBrush3Clicked);

    RefreshHighlight();
}

void UDebugPaintWidget::SelectMaterial(EMaterialType Mat)
{
    ActiveMaterial = Mat;
    RefreshHighlight();
    OnMaterialSelected.ExecuteIfBound(Mat);
}

void UDebugPaintWidget::SelectBrush(int32 Radius)
{
    BrushRadius = Radius;
    RefreshHighlight();
    OnBrushRadiusSelected.ExecuteIfBound(Radius);
}

void UDebugPaintWidget::RefreshHighlight()
{
    static const EMaterialType Mats[MatCount] = {
        EMaterialType::Sand, EMaterialType::Water, EMaterialType::Stone,
        EMaterialType::Wood, EMaterialType::Fire,  EMaterialType::Lava,
    };
    for (int32 i = 0; i < MatCount; ++i)
    {
        if (!MatButtonTexts[i]) continue;
        const bool bActive = (Mats[i] == ActiveMaterial);
        MatButtonTexts[i]->SetColorAndOpacity(FSlateColor(
            bActive ? FLinearColor(0.40f, 0.85f, 1.00f) : FLinearColor(0.85f, 0.88f, 0.95f)));
    }

    static const int32 BrushRadii[BrushCount] = { 0, 1, 2, 4 };
    for (int32 i = 0; i < BrushCount; ++i)
    {
        if (!BrushButtonTexts[i]) continue;
        const bool bActive = (BrushRadii[i] == BrushRadius);
        BrushButtonTexts[i]->SetColorAndOpacity(FSlateColor(
            bActive ? FLinearColor(0.40f, 0.85f, 1.00f) : FLinearColor(0.85f, 0.88f, 0.95f)));
    }
}

void UDebugPaintWidget::OnMat0Clicked() { SelectMaterial(EMaterialType::Sand);  }
void UDebugPaintWidget::OnMat1Clicked() { SelectMaterial(EMaterialType::Water); }
void UDebugPaintWidget::OnMat2Clicked() { SelectMaterial(EMaterialType::Stone); }
void UDebugPaintWidget::OnMat3Clicked() { SelectMaterial(EMaterialType::Wood);  }
void UDebugPaintWidget::OnMat4Clicked() { SelectMaterial(EMaterialType::Fire);  }
void UDebugPaintWidget::OnMat5Clicked() { SelectMaterial(EMaterialType::Lava);  }

void UDebugPaintWidget::OnBrush0Clicked() { SelectBrush(0); }
void UDebugPaintWidget::OnBrush1Clicked() { SelectBrush(1); }
void UDebugPaintWidget::OnBrush2Clicked() { SelectBrush(2); }
void UDebugPaintWidget::OnBrush3Clicked() { SelectBrush(4); }
