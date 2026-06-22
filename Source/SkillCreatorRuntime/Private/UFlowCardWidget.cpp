#include "UFlowCardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UFlowCardWidget::Setup(const FString& InId, const FText& InLabelText, const FLinearColor& CardColor,
                             TFunction<void(const FString&)> InOnClicked,
                             TFunction<void(const FString&)> InOnDeleteClicked,
                             bool bShowDeleteButton)
{
    Id              = InId;
    OnClicked       = MoveTemp(InOnClicked);
    OnDeleteClicked = MoveTemp(InOnDeleteClicked);

    // 整列固定 64px 高（對應 Godot CustomMinimumSize = new Vector2(750, 64)）
    USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>();
    RowSize->SetHeightOverride(64.f);
    WidgetTree->RootWidget = RowSize;

    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    RowSize->SetContent(Row);

    // 卡片本體（點擊 → 選取/進入，對應 Godot card + clickBtn 疊在一起）
    UButton* CardBtn = WidgetTree->ConstructWidget<UButton>();
    CardBtn->SetBackgroundColor(CardColor);
    {
        UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>();
        Lbl->SetText(InLabelText);
        CardBtn->AddChild(Lbl);
    }
    CardBtn->OnClicked.AddDynamic(this, &UFlowCardWidget::HandleCardClicked);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(CardBtn))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
    }

    // 🗑 刪除鈕（對應 Godot delBtn，GameFlowUI.cs:246-251 / 388-393）；W-10 角色創建精靈重用
    // 這個卡片類別時不需要刪除功能，bShowDeleteButton=false 直接跳過整個按鈕。
    if (bShowDeleteButton)
    {
        USizeBox* DelSize = WidgetTree->ConstructWidget<USizeBox>();
        DelSize->SetWidthOverride(52.f);
        UButton* DelBtn = WidgetTree->ConstructWidget<UButton>();
        DelBtn->SetBackgroundColor(FLinearColor(0.38f, 0.10f, 0.10f, 1.f));
        {
            UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
            Txt->SetText(FText::FromString(TEXT("\U0001F5D1")));
            DelBtn->AddChild(Txt);
        }
        DelBtn->OnClicked.AddDynamic(this, &UFlowCardWidget::HandleDeleteClicked);
        DelSize->SetContent(DelBtn);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(DelSize))
            S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    }
}

void UFlowCardWidget::HandleCardClicked()
{
    if (OnClicked) OnClicked(Id);
}

void UFlowCardWidget::HandleDeleteClicked()
{
    if (OnDeleteClicked) OnDeleteClicked(Id);
}
