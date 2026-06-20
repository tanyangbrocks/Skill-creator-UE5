#include "UBlockEditorWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"

void UBlockEditorWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildLayout();
}

void UBlockEditorWidget::BuildLayout()
{
    // 根節點需明確、非零的 desired size（沿用 UGameFlowWidget::BuildLayout 同款做法，
    // 避免 CanvasPanel/Border FullRect anchor 在某些情況下無法解析出可用空間導致整個
    // overlay 不可見/不可點擊）
    const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);

    USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>();
    RootSize->SetWidthOverride(ViewportSize.X);
    RootSize->SetHeightOverride(ViewportSize.Y);
    WidgetTree->RootWidget = RootSize;

    // Godot AbilityEditorUI.cs:126 ColorRect(0.11,0.11,0.14)
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.11f, 0.11f, 0.14f, 0.98f));
    RootSize->SetContent(Background);

    UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>();
    Background->SetContent(MainVBox);

    if (UVerticalBoxSlot* HeaderSlot = MainVBox->AddChildToVerticalBox(BuildHeader()))
        HeaderSlot->SetHorizontalAlignment(HAlign_Fill);

    if (UVerticalBoxSlot* BodySlot = MainVBox->AddChildToVerticalBox(BuildBody()))
    {
        BodySlot->SetHorizontalAlignment(HAlign_Fill);
        BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }
}

UWidget* UBlockEditorWidget::BuildHeader()
{
    // Godot AbilityEditorUI.cs:151-152：Panel(0.16,0.16,0.20)，固定高度 50px
    USizeBox* HeaderSize = WidgetTree->ConstructWidget<USizeBox>();
    HeaderSize->SetHeightOverride(50.f);

    UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>();
    HeaderBg->SetBrushColor(FLinearColor(0.16f, 0.16f, 0.20f, 1.f));
    HeaderSize->SetContent(HeaderBg);

    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    HeaderBg->SetContent(Row);

    // ← 返回（Godot AbilityEditorUI.cs:163-171，84×30）
    BackButton = WidgetTree->ConstructWidget<UButton>();
    BackButton->SetBackgroundColor(FLinearColor(0.20f, 0.20f, 0.28f, 1.f));
    {
        UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
        Txt->SetText(FText::FromString(TEXT("← 返回")));
        BackButton->AddChild(Txt);
    }
    BackButton->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnBackClicked);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(BackButton))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        S->SetPadding(FMargin(4.f, 0.f));
        S->SetVerticalAlignment(VAlign_Center);
    }

    // 技能名稱輸入框（Godot AbilityEditorUI.cs:174-179，180×34）
    SpellNameBox = WidgetTree->ConstructWidget<UEditableTextBox>();
    SpellNameBox->SetHintText(FText::FromString(TEXT("輸入技能整構名稱（必填）")));
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(SpellNameBox))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        S->SetPadding(FMargin(4.f, 0.f));
        S->SetVerticalAlignment(VAlign_Center);
    }

    // 麵包屑（Godot AbilityEditorUI.cs:181-187，預設隱藏，Phase 6 接通）
    BreadcrumbLabel = WidgetTree->ConstructWidget<UTextBlock>();
    BreadcrumbLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.82f, 1.0f)));
    BreadcrumbLabel->SetVisibility(ESlateVisibility::Collapsed);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(BreadcrumbLabel))
        S->SetVerticalAlignment(VAlign_Center);

    // 彈性間隔，把 5 組 dot + 狀態文字推到右側
    UBorder* FlexSpacer = WidgetTree->ConstructWidget<UBorder>();
    FlexSpacer->SetBrushColor(FLinearColor::Transparent);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(FlexSpacer))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    // 5 組技能組切換 dot（Godot AbilityEditorUI.cs:192-208，26×26，間隔 3px）
    UHorizontalBox* DotRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(DotRow))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        S->SetVerticalAlignment(VAlign_Center);
    }

    void (UBlockEditorWidget::*DotHandlers[GroupDotCount])() = {
        &UBlockEditorWidget::OnGroupDot0Clicked, &UBlockEditorWidget::OnGroupDot1Clicked,
        &UBlockEditorWidget::OnGroupDot2Clicked, &UBlockEditorWidget::OnGroupDot3Clicked,
        &UBlockEditorWidget::OnGroupDot4Clicked,
    };
    for (int32 i = 0; i < GroupDotCount; ++i)
    {
        UButton* Dot = WidgetTree->ConstructWidget<UButton>();
        Dot->SetBackgroundColor(FLinearColor(0.50f, 0.50f, 0.60f, 1.f)); // 未啟用色，Phase 7 接 RefreshGroupDots
        {
            UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
            Txt->SetText(FText::AsNumber(i + 1));
            Txt->SetJustification(ETextJustify::Center);
            Dot->AddChild(Txt);
        }
        Dot->OnClicked.AddDynamic(this, DotHandlers[i]);
        if (UHorizontalBoxSlot* S = DotRow->AddChildToHorizontalBox(Dot))
        {
            S->SetPadding(FMargin(1.5f));
            S->SetVerticalAlignment(VAlign_Center);
        }
        GroupDots[i] = Dot;
    }

    // 狀態文字（黃字，Godot 右側狀態提示）
    StatusLabel = WidgetTree->ConstructWidget<UTextBlock>();
    StatusLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.85f, 0.30f)));
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(StatusLabel))
    {
        S->SetPadding(FMargin(8.f, 0.f));
        S->SetVerticalAlignment(VAlign_Center);
    }

    return HeaderSize;
}

UWidget* UBlockEditorWidget::BuildBody()
{
    UHorizontalBox* BodyRow = WidgetTree->ConstructWidget<UHorizontalBox>();

    // 左側 Palette 容器（Godot AbilityEditorUI.cs:420-421，220px，內容於 Phase 2 填入）
    USizeBox* LeftSize = WidgetTree->ConstructWidget<USizeBox>();
    LeftSize->SetWidthOverride(220.f);
    LeftPanel = WidgetTree->ConstructWidget<UBorder>();
    LeftPanel->SetBrushColor(FLinearColor(0.13f, 0.13f, 0.17f, 1.f));
    LeftSize->SetContent(LeftPanel);
    if (UHorizontalBoxSlot* S = BodyRow->AddChildToHorizontalBox(LeftSize))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

    // 中央積木卡片清單容器（內容於 Phase 3 填入）
    CenterScroll = WidgetTree->ConstructWidget<UScrollBox>();
    CenterList = WidgetTree->ConstructWidget<UVerticalBox>();
    CenterScroll->AddChild(CenterList);
    if (UHorizontalBoxSlot* S = BodyRow->AddChildToHorizontalBox(CenterScroll))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    // 右側統計面板容器（Godot AbilityEditorUI.cs:936-937，175px，內容於 Phase 5 填入）
    USizeBox* RightSize = WidgetTree->ConstructWidget<USizeBox>();
    RightSize->SetWidthOverride(175.f);
    RightPanel = WidgetTree->ConstructWidget<UBorder>();
    RightPanel->SetBrushColor(FLinearColor(0.14f, 0.14f, 0.18f, 1.f));
    RightSize->SetContent(RightPanel);
    if (UHorizontalBoxSlot* S = BodyRow->AddChildToHorizontalBox(RightSize))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

    return BodyRow;
}

void UBlockEditorWidget::OnBackClicked()
{
    // Phase 7 補：root 層時若 bIsDirty 要先彈確認對話框（對應 Godot TryExitEditor）
    OnCloseRequested.ExecuteIfBound();
}

void UBlockEditorWidget::OnGroupDot0Clicked() {}
void UBlockEditorWidget::OnGroupDot1Clicked() {}
void UBlockEditorWidget::OnGroupDot2Clicked() {}
void UBlockEditorWidget::OnGroupDot3Clicked() {}
void UBlockEditorWidget::OnGroupDot4Clicked() {}
