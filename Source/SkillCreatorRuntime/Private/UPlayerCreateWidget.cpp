#include "UPlayerCreateWidget.h"
#include "UFlowCardWidget.h"
#include "UStatAllocatorWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableText.h"

static const FName StatKeys[] = { TEXT("Physique"), TEXT("Strength"), TEXT("Endurance"),
                                   TEXT("Agility"), TEXT("Intellect"), TEXT("Charisma"), TEXT("Luck") };
static const TCHAR* StatLabels[] = { TEXT("體魄"), TEXT("肌力"), TEXT("耐力"),
                                      TEXT("敏捷"), TEXT("智慧"), TEXT("魅力"), TEXT("幸運") };

static FString TierLabel(ERaceTier Tier)
{
    switch (Tier)
    {
        case ERaceTier::Yellow:   return TEXT("黃階");
        case ERaceTier::Profound: return TEXT("玄階");
        case ERaceTier::Earth:    return TEXT("地階");
        case ERaceTier::Heaven:   return TEXT("天階");
        default:                  return TEXT("?");
    }
}

// 用 NativeOnInitialized()，不是 NativeConstruct()——同一個專案性的修正（見 UGameFlowWidget
// 的說明、docs/開發血汗錄.md 第 1 案）。
void UPlayerCreateWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();
}

void UPlayerCreateWidget::BuildLayout()
{
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.10f, 1.f));
    WidgetTree->RootWidget = Background;

    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    Background->SetContent(Root);

    UVerticalBox* MainStack = WidgetTree->ConstructWidget<UVerticalBox>();
    if (UOverlaySlot* S = Root->AddChildToOverlay(MainStack))
    {
        S->SetHorizontalAlignment(HAlign_Fill);
        S->SetVerticalAlignment(VAlign_Fill);
    }

    // ── 頂部：標題（左） + 快速建立（右）─────────────────────────────────────
    UHorizontalBox* TopRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    if (UVerticalBoxSlot* S = MainStack->AddChildToVerticalBox(TopRow))
        S->SetPadding(FMargin(20.f, 16.f, 20.f, 8.f));

    StepTitleText = WidgetTree->ConstructWidget<UTextBlock>();
    {
        FSlateFontInfo F = StepTitleText->GetFont();
        F.Size = 26;
        StepTitleText->SetFont(F);
    }
    StepTitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.86f, 0.55f)));
    if (UHorizontalBoxSlot* S = TopRow->AddChildToHorizontalBox(StepTitleText))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetVerticalAlignment(VAlign_Center);
    }

    QuickCreateBtn = WidgetTree->ConstructWidget<UButton>();
    QuickCreateBtn->SetBackgroundColor(FLinearColor(0.30f, 0.22f, 0.10f, 1.f));
    UTextBlock* QcTxt = WidgetTree->ConstructWidget<UTextBlock>();
    QcTxt->SetText(FText::FromString(TEXT("快速建立")));
    QuickCreateBtn->AddChild(QcTxt);
    QuickCreateBtn->OnClicked.AddDynamic(this, &UPlayerCreateWidget::OnQuickCreateClicked);
    TopRow->AddChildToHorizontalBox(QuickCreateBtn);

    // ── 中間：步驟內容（逐步切換可見性，跟 UGameFlowWidget::ShowScreen 同一招）──
    StepStack = WidgetTree->ConstructWidget<UOverlay>();
    if (UVerticalBoxSlot* S = MainStack->AddChildToVerticalBox(StepStack))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetPadding(FMargin(20.f, 0.f, 20.f, 0.f));
    }

    // ── 底部：上一步/重新建立（左） + 下一步/確認/確定創建（右）─────────────
    UHorizontalBox* BottomRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    if (UVerticalBoxSlot* S = MainStack->AddChildToVerticalBox(BottomRow))
        S->SetPadding(FMargin(20.f, 8.f, 20.f, 20.f));

    PrevBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* PrevTxt = WidgetTree->ConstructWidget<UTextBlock>();
    PrevTxt->SetText(FText::FromString(TEXT("上一步")));
    PrevBtn->AddChild(PrevTxt);
    PrevBtn->OnClicked.AddDynamic(this, &UPlayerCreateWidget::OnPrevClicked);
    BottomRow->AddChildToHorizontalBox(PrevBtn);

    USizeBox* MidSpacer = WidgetTree->ConstructWidget<USizeBox>();
    if (UHorizontalBoxSlot* S = BottomRow->AddChildToHorizontalBox(MidSpacer))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    NextBtn = WidgetTree->ConstructWidget<UButton>();
    NextBtn->SetBackgroundColor(FLinearColor(0.18f, 0.35f, 0.18f, 1.f));
    NextBtnText = WidgetTree->ConstructWidget<UTextBlock>();
    NextBtnText->SetText(FText::FromString(TEXT("下一步")));
    NextBtn->AddChild(NextBtnText);
    NextBtn->OnClicked.AddDynamic(this, &UPlayerCreateWidget::OnNextClicked);
    BottomRow->AddChildToHorizontalBox(NextBtn);

    // ── 警告彈窗（步驟 7：創建能力點不足，對應使用者規格第 7 點）─────────────
    UBorder* WarnBg = WidgetTree->ConstructWidget<UBorder>();
    WarnBg->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.75f));

    UBorder* WarnPanel = WidgetTree->ConstructWidget<UBorder>();
    WarnPanel->SetBrushColor(FLinearColor(0.16f, 0.10f, 0.10f, 1.f));
    WarnPanel->SetPadding(FMargin(24.f, 20.f));
    if (UBorderSlot* BS = Cast<UBorderSlot>(WarnBg->SetContent(WarnPanel)))
    {
        BS->SetHorizontalAlignment(HAlign_Center);
        BS->SetVerticalAlignment(VAlign_Center);
    }

    UVerticalBox* WarnInner = WidgetTree->ConstructWidget<UVerticalBox>();
    WarnPanel->SetContent(WarnInner);

    WarningText = WidgetTree->ConstructWidget<UTextBlock>();
    WarningText->SetText(FText::FromString(TEXT("創建能力點數不足，請回上一步調整分配。")));
    if (UVerticalBoxSlot* S = WarnInner->AddChildToVerticalBox(WarningText))
        S->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

    UButton* WarnOkBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* WarnOkTxt = WidgetTree->ConstructWidget<UTextBlock>();
    WarnOkTxt->SetText(FText::FromString(TEXT("確定")));
    WarnOkBtn->AddChild(WarnOkTxt);
    WarnOkBtn->OnClicked.AddDynamic(this, &UPlayerCreateWidget::OnWarningOkClicked);
    if (UVerticalBoxSlot* S = WarnInner->AddChildToVerticalBox(WarnOkBtn))
        S->SetHorizontalAlignment(HAlign_Center);

    WarningOverlay = WarnBg;
    if (UOverlaySlot* S = Root->AddChildToOverlay(WarningOverlay))
    {
        S->SetHorizontalAlignment(HAlign_Fill);
        S->SetVerticalAlignment(VAlign_Fill);
    }
    WarningOverlay->SetVisibility(ESlateVisibility::Collapsed);

    RegisterSteps();
    StepWidgets.SetNum(Steps.Num());
    ShowStep(0);
}

// ── 步驟陣列：容易增減/調整順序，見計畫文件 2.2 ────────────────────────────
void UPlayerCreateWidget::RegisterSteps()
{
    Steps.Empty();
    Steps.Add({ INVTEXT("1. 輸入角色名字"),       [this] { return BuildStepName(); },       nullptr, true });
    Steps.Add({ INVTEXT("2. 設定創建能力點總數"), [this] { return BuildStepPoints(); },     nullptr, true });
    Steps.Add({ INVTEXT("3. 設定種族"),           [this] { return BuildStepRace(); },
                TFunction<bool()>([this] { return !SelectedRaceId.IsNone(); }), true });
    Steps.Add({ INVTEXT("4. 設定外貌"),           [this] { return BuildBlankStep(); },       nullptr, true });
    Steps.Add({ INVTEXT("5. 設定喜好"),           [this] { return BuildBlankStep(); },       nullptr, true });
    Steps.Add({ INVTEXT("6. 設定基礎能力點"),     [this] { return BuildStepBaseStats(); },   nullptr, true });
    Steps.Add({ INVTEXT("7. 設定負面效果"),       [this] { return BuildBlankStep(); },       nullptr, true });
    Steps.Add({ INVTEXT("8. 抽取靈魂萬象"),       [this] { return BuildBlankStep(); },       nullptr, true });
    Steps.Add({ INVTEXT("9. 抽取體質"),           [this] { return BuildBlankStep(); },       nullptr, true });
    Steps.Add({ INVTEXT("10. 心靈眾影響"),        [this] { return BuildBlankStep(); },       nullptr, true });
    Steps.Add({ INVTEXT("11. 最終角色資訊確認"),  [this] { return BuildStepConfirmCard(); }, nullptr, false });
}

void UPlayerCreateWidget::ShowStep(int32 Index)
{
    if (!Steps.IsValidIndex(Index)) return;
    CurrentStepIndex = Index;

    if (!StepWidgets[Index])
    {
        UWidget* W = Steps[Index].Build();
        StepWidgets[Index] = W;
        if (UOverlaySlot* S = StepStack->AddChildToOverlay(W))
        {
            S->SetHorizontalAlignment(HAlign_Fill);
            S->SetVerticalAlignment(VAlign_Fill);
        }
    }

    for (int32 i = 0; i < StepWidgets.Num(); ++i)
        if (StepWidgets[i])
            StepWidgets[i]->SetVisibility(i == Index ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    // 資訊卡每次顯示都重新整理（從某步驟編輯完跳回來，內容可能變了）
    if (Index == Steps.Num() - 1)
        RefreshConfirmCard();

    RefreshChrome();
}

void UPlayerCreateWidget::RefreshChrome()
{
    if (!Steps.IsValidIndex(CurrentStepIndex)) return;
    const FPlayerCreateStep& Step = Steps[CurrentStepIndex];
    const bool bLast = CurrentStepIndex == Steps.Num() - 1;

    if (StepTitleText) StepTitleText->SetText(Step.Title);
    if (QuickCreateBtn)
        QuickCreateBtn->SetVisibility(Step.bShowQuickCreate ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    if (PrevBtn)
        if (UTextBlock* T = Cast<UTextBlock>(PrevBtn->GetChildAt(0)))
            T->SetText(bLast ? FText::FromString(TEXT("重新建立")) : FText::FromString(TEXT("上一步")));

    if (NextBtnText)
        NextBtnText->SetText(bLast ? FText::FromString(TEXT("確定創建"))
                                    : (ReturnToStepIndex >= 0 ? FText::FromString(TEXT("確認"))
                                                               : FText::FromString(TEXT("下一步"))));

    const bool bCanAdvance = !Step.CanAdvance || Step.CanAdvance();
    if (NextBtn) NextBtn->SetIsEnabled(bLast || bCanAdvance);
}

void UPlayerCreateWidget::OnPrevClicked()
{
    const bool bLast = CurrentStepIndex == Steps.Num() - 1;
    if (bLast)
    {
        // 「重新建立」：回到步驟 1，清空所有暫存創建狀態
        ResetCreationState();
        ShowStep(0);
        return;
    }
    if (CurrentStepIndex == 0)
    {
        // 步驟 1 按「上一步」= 放棄創建，回角色選擇畫面（每個流程都要有上一步，這裡兼當取消鍵）
        if (OnCancelled) OnCancelled();
        return;
    }
    ShowStep(CurrentStepIndex - 1);
}

void UPlayerCreateWidget::OnNextClicked()
{
    if (ReturnToStepIndex >= 0)
    {
        const int32 Target = ReturnToStepIndex;
        ReturnToStepIndex = -1;
        ShowStep(Target);
        return;
    }

    const bool bLast = CurrentStepIndex == Steps.Num() - 1;
    if (bLast)
    {
        OnConfirmCreateClicked();
        return;
    }

    const FPlayerCreateStep& Step = Steps[CurrentStepIndex];
    if (Step.CanAdvance && !Step.CanAdvance()) return;

    // 步驟 7（index 6）：創建能力點 < 0 時擋下並跳警告視窗（對應使用者規格第 7 點）
    if (CurrentStepIndex == 6 && GetRemainingPoints() < 0)
    {
        if (WarningOverlay) WarningOverlay->SetVisibility(ESlateVisibility::Visible);
        return;
    }

    ShowStep(CurrentStepIndex + 1);
}

void UPlayerCreateWidget::OnWarningOkClicked()
{
    if (WarningOverlay) WarningOverlay->SetVisibility(ESlateVisibility::Collapsed);
}

void UPlayerCreateWidget::ResetAndShowFirstStep()
{
    ResetCreationState();
    ShowStep(0);
}

void UPlayerCreateWidget::ResetCreationState()
{
    CreationName.Empty();
    CreationPointsTotal = 0;
    CreationPointsSpent = 0;
    SelectedRaceId = NAME_None;
    BaseStatAllocations.Empty();
    ReturnToStepIndex = -1;

    // 已快取的步驟畫面要丟掉重建（名字輸入框要清空、種族清單已選狀態要重置等），
    // 簡化作法：從 StepStack 移除、StepWidgets 清空，ShowStep() 會重新呼叫 Build()。
    for (TObjectPtr<UWidget>& W : StepWidgets)
    {
        if (W) W->RemoveFromParent();
        W = nullptr;
    }
    StatRows.Empty();
}

void UPlayerCreateWidget::OnQuickCreateClicked()
{
    DoQuickCreateRandomize();
    ShowStep(Steps.Num() - 1);
}

void UPlayerCreateWidget::DoQuickCreateRandomize()
{
    // 對應使用者規格：「點擊該按鈕後，要能自動、隨機地走完流程1~10，直接跳至流程11」。
    // 空白流程（4/5/7/8/9/10）天然不需要隨機化，只有 1/2/3/6 真的有東西要隨機填。
    if (CreationName.IsEmpty()) CreationName = TEXT("旅者");
    if (CreationPointsTotal == 0) CreationPointsTotal = FMath::RandRange(50, 100);

    if (SelectedRaceId.IsNone())
    {
        TArray<FName> Affordable;
        for (const FRaceSystemDefinition& SysDef : FRaceRegistry::AllSystems())
            for (const FRaceDefinition* Def : FRaceRegistry::RacesInSystem(SysDef.SystemId))
                if (FRaceRegistry::CostForTier(Def->Tier) <= GetRemainingPoints())
                    Affordable.Add(Def->Id);
        if (Affordable.Num() > 0)
            SelectRace(Affordable[FMath::RandRange(0, Affordable.Num() - 1)]);
    }

    int32 Guard = 0;
    while (GetRemainingPoints() > 0 && Guard++ < 10000)
        AdjustStat(StatKeys[FMath::RandRange(0, 6)], 1);
}

// ════════════════════════════════════════════════════════════════════════
// 步驟 1：輸入角色名字
// ════════════════════════════════════════════════════════════════════════
UWidget* UPlayerCreateWidget::BuildStepName()
{
    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();

    UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>();
    Lbl->SetText(FText::FromString(TEXT("角色名稱：")));
    if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Lbl))
        S->SetPadding(FMargin(0.f, 60.f, 0.f, 8.f));

    NameInput = WidgetTree->ConstructWidget<UEditableText>();
    NameInput->SetHintText(FText::FromString(TEXT("旅者")));
    NameInput->SetText(FText::FromString(CreationName));
    Box->AddChildToVerticalBox(NameInput);

    return Box;
}

// ════════════════════════════════════════════════════════════════════════
// 步驟 2：設定創建能力點總數
// ════════════════════════════════════════════════════════════════════════
UWidget* UPlayerCreateWidget::BuildStepPoints()
{
    if (CreationPointsTotal == 0)
        CreationPointsTotal = FMath::RandRange(50, 100);

    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();

    PointsTotalLabel = WidgetTree->ConstructWidget<UTextBlock>();
    {
        FSlateFontInfo F = PointsTotalLabel->GetFont();
        F.Size = 22;
        PointsTotalLabel->SetFont(F);
    }
    PointsTotalLabel->SetText(FText::FromString(FString::Printf(TEXT("創建能力點：%d"), CreationPointsTotal)));
    if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(PointsTotalLabel))
        S->SetPadding(FMargin(0.f, 60.f, 0.f, 16.f));

    UButton* MaxBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* MaxTxt = WidgetTree->ConstructWidget<UTextBlock>();
    MaxTxt->SetText(FText::FromString(TEXT("設置最高能力點")));
    MaxBtn->AddChild(MaxTxt);
    MaxBtn->OnClicked.AddDynamic(this, &UPlayerCreateWidget::OnSetMaxPointsClicked);
    Box->AddChildToVerticalBox(MaxBtn);

    return Box;
}

void UPlayerCreateWidget::OnSetMaxPointsClicked()
{
    CreationPointsTotal = 100;
    if (PointsTotalLabel)
        PointsTotalLabel->SetText(FText::FromString(FString::Printf(TEXT("創建能力點：%d"), CreationPointsTotal)));
}

// ════════════════════════════════════════════════════════════════════════
// 步驟 3：設定種族
// ════════════════════════════════════════════════════════════════════════
UWidget* UPlayerCreateWidget::BuildStepRace()
{
    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();

    UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>();
    Hint->SetText(FText::FromString(TEXT("選擇所屬體系，再從清單中選定種族：")));
    Hint->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.70f)));
    if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Hint))
        S->SetPadding(FMargin(0.f, 12.f, 0.f, 8.f));

    RaceSystemBubbles = WidgetTree->ConstructWidget<UWrapBox>();
    RaceSystemBubbles->SetInnerSlotPadding(FVector2D(6.f, 6.f));
    if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(RaceSystemBubbles))
        S->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    RaceSpentLabel = WidgetTree->ConstructWidget<UTextBlock>();
    RaceSpentLabel->SetText(FText::FromString(TEXT("尚未選擇種族")));
    if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(RaceSpentLabel))
        S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

    RaceListScroll = WidgetTree->ConstructWidget<UScrollBox>();
    if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(RaceListScroll))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    RaceListContainer = WidgetTree->ConstructWidget<UVerticalBox>();
    RaceListScroll->AddChild(RaceListContainer);

    RefreshRaceSystemBubbles();

    return Box;
}

void UPlayerCreateWidget::RefreshRaceSystemBubbles()
{
    if (!RaceSystemBubbles) return;
    RaceSystemBubbles->ClearChildren();

    for (const FRaceSystemDefinition& SysDef : FRaceRegistry::AllSystems())
    {
        UFlowCardWidget* Bubble = WidgetTree->ConstructWidget<UFlowCardWidget>();
        const FName SystemId = SysDef.SystemId;
        Bubble->Setup(SysDef.SystemId.ToString(), SysDef.DisplayName,
            FLinearColor(0.16f, 0.16f, 0.22f, 1.f),
            [this](const FString& Id) { ShowRaceList(FName(*Id)); },
            [](const FString&) {},
            /*bShowDeleteButton=*/false);

        USizeBox* Wrap = WidgetTree->ConstructWidget<USizeBox>();
        Wrap->SetWidthOverride(150.f);
        Wrap->SetHeightOverride(56.f);
        Wrap->SetContent(Bubble);
        RaceSystemBubbles->AddChildToWrapBox(Wrap);
    }
}

void UPlayerCreateWidget::ShowRaceList(FName SystemId)
{
    if (!RaceListContainer) return;
    RaceListContainer->ClearChildren();

    for (const FRaceDefinition* Def : FRaceRegistry::RacesInSystem(SystemId))
    {
        UFlowCardWidget* Card = WidgetTree->ConstructWidget<UFlowCardWidget>();
        const int32 Cost = FRaceRegistry::CostForTier(Def->Tier);
        const FText Label = FText::FromString(FString::Printf(TEXT("%s（%s，%d 點）— %s"),
            *Def->DisplayName.ToString(), *TierLabel(Def->Tier), Cost, *Def->Description.ToString()));
        const FName RaceId = Def->Id;
        Card->Setup(Def->Id.ToString(), Label, FLinearColor(0.12f, 0.14f, 0.18f, 1.f),
            [this](const FString& Id) { SelectRace(FName(*Id)); },
            [](const FString&) {},
            /*bShowDeleteButton=*/false);

        if (UVerticalBoxSlot* S = RaceListContainer->AddChildToVerticalBox(Card))
            S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
    }
}

void UPlayerCreateWidget::SelectRace(FName RaceId)
{
    const FRaceDefinition* NewDef = FRaceRegistry::Find(RaceId);
    if (!NewDef) return;

    // 重選種族：先退回舊種族花費，避免重複扣點
    if (const FRaceDefinition* OldDef = FRaceRegistry::Find(SelectedRaceId))
        CreationPointsSpent -= FRaceRegistry::CostForTier(OldDef->Tier);

    SelectedRaceId = RaceId;
    CreationPointsSpent += FRaceRegistry::CostForTier(NewDef->Tier);

    if (RaceSpentLabel)
        RaceSpentLabel->SetText(FText::FromString(FString::Printf(TEXT("已選種族：%s（剩餘 %d 點）"),
            *NewDef->DisplayName.ToString(), GetRemainingPoints())));

    RefreshChrome();
}

// ════════════════════════════════════════════════════════════════════════
// 步驟 4/5/7/8/9/10：空白流程（PS：只有標題+下一步，內容空白）
// ════════════════════════════════════════════════════════════════════════
UWidget* UPlayerCreateWidget::BuildBlankStep()
{
    return WidgetTree->ConstructWidget<UBorder>();
}

// ════════════════════════════════════════════════════════════════════════
// 步驟 6：設定基礎能力點（仿「人生重開模擬器」：剩餘點數 + 7 列 −/數字/+）
// ════════════════════════════════════════════════════════════════════════
UWidget* UPlayerCreateWidget::BuildStepBaseStats()
{
    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();

    RemainingPointsLabel = WidgetTree->ConstructWidget<UTextBlock>();
    {
        FSlateFontInfo F = RemainingPointsLabel->GetFont();
        F.Size = 24;
        RemainingPointsLabel->SetFont(F);
    }
    RemainingPointsLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.95f, 0.68f)));
    RemainingPointsLabel->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(RemainingPointsLabel))
    {
        S->SetPadding(FMargin(0.f, 24.f, 0.f, 20.f));
        S->SetHorizontalAlignment(HAlign_Center);
    }

    StatRows.Empty();
    for (int32 i = 0; i < 7; ++i)
    {
        const FName Key = StatKeys[i];
        UStatAllocatorWidget* Row = WidgetTree->ConstructWidget<UStatAllocatorWidget>();
        Row->Setup(FText::FromString(StatLabels[i]), BaseStatAllocations.FindOrAdd(Key),
            [this, Key](int32 Delta) { AdjustStat(Key, Delta); },
            [this, Key](int32 NewValue) { SetStatValue(Key, NewValue); });
        StatRows.Add(Key, Row);
        if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Row))
        {
            S->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
            S->SetHorizontalAlignment(HAlign_Center);
        }
    }

    // 初始顯示（套用目前剩餘點數決定 +/- 按鈕是否可按）
    for (const FName& Key : StatKeys) RefreshStatRow(Key);
    if (RemainingPointsLabel)
        RemainingPointsLabel->SetText(FText::FromString(FString::Printf(TEXT("剩餘創建能力點：%d"), GetRemainingPoints())));

    // 使用者要求「基礎能力點介面放中間、大一點」：外層每個步驟的 Overlay slot 是
    // HAlign_Fill/VAlign_Fill（ShowStep() 對所有步驟統一設定，不能單獨改），所以要靠
    // 自己內層再包一層置中容器，否則 VerticalBox 預設貼齊左上角。SizeBox 固定一個比較
    // 寬的內容寬度，讓整組「−輸入框+」看起來更大、更好點。
    USizeBox* SizeWrapper = WidgetTree->ConstructWidget<USizeBox>();
    SizeWrapper->SetMinDesiredWidth(420.f);
    SizeWrapper->SetContent(Box);

    UOverlay* CenterWrapper = WidgetTree->ConstructWidget<UOverlay>();
    if (UOverlaySlot* S = CenterWrapper->AddChildToOverlay(SizeWrapper))
    {
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetVerticalAlignment(VAlign_Center);
    }

    return CenterWrapper;
}

void UPlayerCreateWidget::AdjustStat(FName StatKey, int32 Delta)
{
    int32& Cur = BaseStatAllocations.FindOrAdd(StatKey);
    if (Delta > 0 && GetRemainingPoints() <= 0) return;
    if (Delta < 0 && Cur <= 0) return;

    Cur += Delta;
    CreationPointsSpent += Delta;

    for (const FName& Key : StatKeys) RefreshStatRow(Key);
    if (RemainingPointsLabel)
        RemainingPointsLabel->SetText(FText::FromString(FString::Printf(TEXT("剩餘創建能力點：%d"), GetRemainingPoints())));
}

void UPlayerCreateWidget::SetStatValue(FName StatKey, int32 NewValue)
{
    int32& Cur = BaseStatAllocations.FindOrAdd(StatKey);
    NewValue = FMath::Max(NewValue, 0);
    const int32 MaxAffordable = Cur + GetRemainingPoints();
    NewValue = FMath::Min(NewValue, MaxAffordable);

    const int32 Delta = NewValue - Cur;
    Cur = NewValue;
    CreationPointsSpent += Delta;

    for (const FName& Key : StatKeys) RefreshStatRow(Key);
    if (RemainingPointsLabel)
        RemainingPointsLabel->SetText(FText::FromString(FString::Printf(TEXT("剩餘創建能力點：%d"), GetRemainingPoints())));
}

void UPlayerCreateWidget::RefreshStatRow(FName StatKey)
{
    TObjectPtr<UStatAllocatorWidget>* Row = StatRows.Find(StatKey);
    if (!Row || !*Row) return;
    const int32 Value = BaseStatAllocations.FindOrAdd(StatKey);
    (*Row)->UpdateDisplay(Value, Value > 0, GetRemainingPoints() > 0);
}

// ════════════════════════════════════════════════════════════════════════
// 步驟 11：最終角色資訊確認
// ════════════════════════════════════════════════════════════════════════
UWidget* UPlayerCreateWidget::BuildStepConfirmCard()
{
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    ConfirmCardContainer = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(ConfirmCardContainer);
    return Scroll;
}

static UWidget* MakeConfirmRow(UWidgetTree* Tree, const FString& Text, UButton** OutEditBtn)
{
    UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>();

    UTextBlock* Txt = Tree->ConstructWidget<UTextBlock>();
    Txt->SetText(FText::FromString(Text));
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Txt))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetVerticalAlignment(VAlign_Center);
    }

    if (OutEditBtn)
    {
        UButton* EditBtn = Tree->ConstructWidget<UButton>();
        UTextBlock* EditTxt = Tree->ConstructWidget<UTextBlock>();
        EditTxt->SetText(FText::FromString(TEXT("編輯")));
        EditBtn->AddChild(EditTxt);
        Row->AddChildToHorizontalBox(EditBtn);
        *OutEditBtn = EditBtn;
    }

    return Row;
}

void UPlayerCreateWidget::RefreshConfirmCard()
{
    if (!ConfirmCardContainer) return;
    ConfirmCardContainer->ClearChildren();

    UButton* EditBtn = nullptr;

    const FString Name = CreationName.IsEmpty() ? TEXT("旅者") : CreationName;
    UWidget* NameRow = MakeConfirmRow(WidgetTree, FString::Printf(TEXT("角色名稱：%s"), *Name), &EditBtn);
    EditBtn->OnClicked.AddDynamic(this, &UPlayerCreateWidget::OnEditNameClicked);
    if (UVerticalBoxSlot* S = ConfirmCardContainer->AddChildToVerticalBox(NameRow))
        S->SetPadding(FMargin(0.f, 8.f));

    EditBtn = nullptr;
    UWidget* PointsRow = MakeConfirmRow(WidgetTree,
        FString::Printf(TEXT("創建能力點：%d（已花費 %d，剩餘 %d）"), CreationPointsTotal, CreationPointsSpent, GetRemainingPoints()),
        &EditBtn);
    EditBtn->OnClicked.AddDynamic(this, &UPlayerCreateWidget::OnEditPointsClicked);
    if (UVerticalBoxSlot* S = ConfirmCardContainer->AddChildToVerticalBox(PointsRow))
        S->SetPadding(FMargin(0.f, 8.f));

    EditBtn = nullptr;
    const FRaceDefinition* RaceDef = FRaceRegistry::Find(SelectedRaceId);
    UWidget* RaceRow = MakeConfirmRow(WidgetTree,
        RaceDef ? FString::Printf(TEXT("種族：%s（%s）"), *RaceDef->DisplayName.ToString(), *TierLabel(RaceDef->Tier))
                : TEXT("種族：（未設定）"),
        &EditBtn);
    EditBtn->OnClicked.AddDynamic(this, &UPlayerCreateWidget::OnEditRaceClicked);
    if (UVerticalBoxSlot* S = ConfirmCardContainer->AddChildToVerticalBox(RaceRow))
        S->SetPadding(FMargin(0.f, 8.f));

    EditBtn = nullptr;
    FString StatsLine = TEXT("基礎能力點：");
    for (int32 i = 0; i < 7; ++i)
        StatsLine += FString::Printf(TEXT("%s %d　"), StatLabels[i], BaseStatAllocations.FindRef(StatKeys[i]));
    UWidget* StatsRow = MakeConfirmRow(WidgetTree, StatsLine, &EditBtn);
    EditBtn->OnClicked.AddDynamic(this, &UPlayerCreateWidget::OnEditStatsClicked);
    if (UVerticalBoxSlot* S = ConfirmCardContainer->AddChildToVerticalBox(StatsRow))
        S->SetPadding(FMargin(0.f, 8.f));

    // 空白流程的步驟（外貌/喜好/負面效果/靈魂萬象/體質/心靈眾影響）目前沒有內容，不列出
    // （對應使用者規格第 11 點：「空白流程的項目就不列」）。
}

void UPlayerCreateWidget::JumpToStepForEdit(int32 StepIndex)
{
    ReturnToStepIndex = Steps.Num() - 1; // 資訊卡是最後一步
    ShowStep(StepIndex);
}

void UPlayerCreateWidget::OnEditNameClicked()   { JumpToStepForEdit(0); }
void UPlayerCreateWidget::OnEditPointsClicked() { JumpToStepForEdit(1); }
void UPlayerCreateWidget::OnEditRaceClicked()   { JumpToStepForEdit(2); }
void UPlayerCreateWidget::OnEditStatsClicked()  { JumpToStepForEdit(5); }

void UPlayerCreateWidget::OnConfirmCreateClicked()
{
    if (NameInput && !NameInput->GetText().IsEmpty())
        CreationName = NameInput->GetText().ToString().TrimStartAndEnd();
    if (CreationName.IsEmpty())
        CreationName = TEXT("旅者");

    FCharacterSaveData Data;
    Data.CharacterName    = CreationName;
    Data.RaceId           = SelectedRaceId;
    Data.BasePoint_Physique  = BaseStatAllocations.FindRef(TEXT("Physique"));
    Data.BasePoint_Strength  = BaseStatAllocations.FindRef(TEXT("Strength"));
    Data.BasePoint_Endurance = BaseStatAllocations.FindRef(TEXT("Endurance"));
    Data.BasePoint_Agility   = BaseStatAllocations.FindRef(TEXT("Agility"));
    Data.BasePoint_Intellect = BaseStatAllocations.FindRef(TEXT("Intellect"));
    Data.BasePoint_Charisma  = BaseStatAllocations.FindRef(TEXT("Charisma"));
    Data.BasePoint_Luck      = BaseStatAllocations.FindRef(TEXT("Luck"));

    if (OnFinished) OnFinished(Data);
}
