#include "UPlayerSettingsWidget.h"
#include "UInputSettingsWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CheckBox.h"
#include "Blueprint/WidgetTree.h"

void UPlayerSettingsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // Stage 4 完整實作；目前先建立基本骨架（操作細節 checkboxes）
    auto* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    WidgetTree->RootWidget = Root;

    // ── 操作細節 ──────────────────────────────────────────────────
    auto AddLabel = [&](const FString& Text)
    {
        auto* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Lbl->SetText(FText::FromString(Text));
        Lbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.1f, 0.1f, 0.2f)));
        UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(Root->AddChild(Lbl));
        if (S) S->SetPadding(FMargin(20.f, 16.f, 0.f, 4.f));
    };
    auto AddCheck = [&](const FString& Text, TObjectPtr<UCheckBox>& OutCheck)
    {
        auto* Row = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
        Row->SetCheckedState(ECheckBoxState::Unchecked);
        // Label as content
        auto* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Lbl->SetText(FText::FromString(Text));
        Lbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.1f, 0.1f, 0.2f)));
        Row->SetContent(Lbl);
        UVerticalBoxSlot* S = Cast<UVerticalBoxSlot>(Root->AddChild(Row));
        if (S) S->SetPadding(FMargin(28.f, 4.f));
        OutCheck = Row;
    };

    AddLabel(TEXT("◎ 操作細節"));
    AddCheck(TEXT("長按連放"), HoldCheck);
    AddCheck(TEXT("完美移除放置"), PerfectCheck);

    if (HoldCheck)
        HoldCheck->OnCheckStateChanged.AddDynamic(this, &UPlayerSettingsWidget::OnHoldToggled);
    if (PerfectCheck)
        PerfectCheck->OnCheckStateChanged.AddDynamic(this, &UPlayerSettingsWidget::OnPerfectToggled);

    // ── 鍵位設定（嵌入 UInputSettingsWidget）────────────────────────
    AddLabel(TEXT("◎ 鍵位設定"));

    APlayerController* PC = GetOwningPlayer();
    if (PC)
    {
        KeybindSection = CreateWidget<UInputSettingsWidget>(PC, UInputSettingsWidget::StaticClass());
        if (KeybindSection)
        {
            UVerticalBoxSlot* KS = Cast<UVerticalBoxSlot>(Root->AddChild(KeybindSection));
            if (KS)
            {
                KS->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
                KS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            }
        }
    }
}

void UPlayerSettingsWidget::SyncState(bool bHold, bool bPerfect)
{
    if (HoldCheck)
        HoldCheck->SetCheckedState(bHold   ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    if (PerfectCheck)
        PerfectCheck->SetCheckedState(bPerfect ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void UPlayerSettingsWidget::OnHoldToggled(bool b)
{
    OnHoldToPlaceChanged.ExecuteIfBound(b);
}

void UPlayerSettingsWidget::OnPerfectToggled(bool b)
{
    OnPerfectRemoveChanged.ExecuteIfBound(b);
}
