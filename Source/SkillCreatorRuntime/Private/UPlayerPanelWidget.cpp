#include "UPlayerPanelWidget.h"
#include "UStatsWidget.h"
#include "USpellListWidget.h"
#include "UOccupationWidget.h"
#include "UInnerWorldWidget.h"
#include "UGuideWidget.h"
#include "UCompendiumWidget.h"
#include "UPlayerSettingsWidget.h"
#include "CharacterStats.h"
#include "UEquipmentComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"

// ── 色彩常數 ─────────────────────────────────────────────────────────────────
static const FLinearColor kPanelBg      { 0.96f, 0.97f, 1.00f, 0.98f };
static const FLinearColor kHeaderBg     { 0.92f, 0.94f, 0.98f, 1.00f };
static const FLinearColor kTabActive    { 0.18f, 0.37f, 0.72f, 1.00f };
static const FLinearColor kTabInactive  { 0.60f, 0.60f, 0.65f, 1.00f };
static const FLinearColor kUnderlineOn  { 0.18f, 0.37f, 0.72f, 1.00f };
static const FLinearColor kUnderlineOff { 0.00f, 0.00f, 0.00f, 0.00f };
static const FLinearColor kCircleBg     { 0.22f, 0.42f, 0.78f, 1.00f };
static const FLinearColor kDivider      { 0.20f, 0.37f, 0.72f, 1.00f };
static const FLinearColor kContentBg    { 1.00f, 1.00f, 1.00f, 1.00f };

// ── NativeOnInitialized ────────────────────────────────────────────────────

void UPlayerPanelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();
    CreateSubWidgets();
    SwitchPage(EPage::Stats);
}

// ── 建構 ────────────────────────────────────────────────────────────────────

void UPlayerPanelWidget::BuildLayout()
{
    // Root canvas（由 ASkillCreatorHUD::BeginPlay 透過 SetAnchorsInViewport 撐滿視窗）
    auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = Root;

    // 整體背景 Border（佔滿 canvas，留 2% 邊距）
    auto* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    BG->SetBrushColor(kPanelBg);
    BG->SetPadding(FMargin(0.f));
    {
        auto* CSlot = Cast<UCanvasPanelSlot>(Root->AddChild(BG));
        CSlot->SetAnchors(FAnchors(0.02f, 0.02f, 0.98f, 0.98f));
        CSlot->SetOffsets(FMargin(0.f));
        CSlot->SetAlignment(FVector2D::ZeroVector);
    }

    // 主 VBox（填滿 BG）
    auto* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    BG->SetContent(VBox);

    BuildNormalHeader(VBox);
    BuildPageHeader(VBox);

    // 分隔線（USizeBox 控高度 2px）
    auto* DividerBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    DividerBox->SetMinDesiredHeight(2.f);
    auto* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Divider->SetBrushColor(kDivider);
    Divider->SetPadding(FMargin(0.f));
    DividerBox->SetContent(Divider);
    {
        auto* S = Cast<UVerticalBoxSlot>(VBox->AddChild(DividerBox));
        if (S) { S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); S->SetHorizontalAlignment(HAlign_Fill); }
    }

    BuildContentArea(VBox);
}

// ── 一般頁首（4 Tab + 3 圓形按鈕） ─────────────────────────────────────────

void UPlayerPanelWidget::BuildNormalHeader(UVerticalBox* Root)
{
    // Header 背景（USizeBox 控最小高度 72px）
    auto* NHBox_SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    NHBox_SizeBox->SetMinDesiredHeight(72.f);
    {
        auto* S = Cast<UVerticalBoxSlot>(Root->AddChild(NHBox_SizeBox));
        if (S) { S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); S->SetHorizontalAlignment(HAlign_Fill); }
    }
    auto* HdrBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    HdrBorder->SetBrushColor(kHeaderBg);
    HdrBorder->SetPadding(FMargin(0.f));
    NHBox_SizeBox->SetContent(HdrBorder);
    NormalHeaderBox = NHBox_SizeBox;

    auto* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    HdrBorder->SetContent(HBox);
    NormalHeader = HBox;

    // ── 4 個 Tab 按鈕 ────────────────────────────────────────────────────
    static const TCHAR* TabNames[] = { TEXT("個人資訊面板"), TEXT("職業能力"), TEXT("技能創建空間"), TEXT("內部空間") };
    void (UPlayerPanelWidget::* TabCallbacks[])() = {
        &UPlayerPanelWidget::OnTabStats,
        &UPlayerPanelWidget::OnTabOccupation,
        &UPlayerPanelWidget::OnTabSpellEditor,
        &UPlayerPanelWidget::OnTabInnerWorld,
    };

    for (int32 i = 0; i < 4; ++i)
    {
        // 每個 Tab = VBox（按鈕 + 底線）
        auto* TabVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        {
            auto* HS = Cast<UHorizontalBoxSlot>(HBox->AddChild(TabVBox));
            if (HS) { HS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); HS->SetVerticalAlignment(VAlign_Bottom); }
        }

        // 透明按鈕
        auto* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        FButtonStyle BtnStyle = Btn->GetStyle();
        BtnStyle.Normal.DrawAs   = ESlateBrushDrawType::NoDrawType;
        BtnStyle.Hovered.DrawAs  = ESlateBrushDrawType::NoDrawType;
        BtnStyle.Pressed.DrawAs  = ESlateBrushDrawType::NoDrawType;
        Btn->SetStyle(BtnStyle);
        Btn->OnClicked.AddDynamic(this, TabCallbacks[i]);
        {
            auto* VS = Cast<UVerticalBoxSlot>(TabVBox->AddChild(Btn));
            if (VS) { VS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); VS->SetPadding(FMargin(24.f, 14.f, 24.f, 10.f)); }
        }
        TabButtons.Add(Btn);

        // Tab 文字
        auto* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Lbl->SetText(FText::FromString(TabNames[i]));
        Lbl->SetColorAndOpacity(FSlateColor(kTabInactive));
        FSlateFontInfo Font = Lbl->GetFont();
        Font.Size = 16;
        Lbl->SetFont(Font);
        Btn->SetContent(Lbl);

        // 底線（3px 高，用 USizeBox 控高度）
        auto* ULBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        ULBox->SetHeightOverride(3.f);
        auto* UL = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        UL->SetBrushColor(kUnderlineOff);
        UL->SetPadding(FMargin(0.f));
        ULBox->SetContent(UL);
        {
            auto* VS = Cast<UVerticalBoxSlot>(TabVBox->AddChild(ULBox));
            if (VS) { VS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); VS->SetHorizontalAlignment(HAlign_Fill); }
        }
        TabUnderlines.Add(UL);
    }

    // Spacer（把右側按鈕推到右邊）
    auto* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
    {
        auto* HS = Cast<UHorizontalBoxSlot>(HBox->AddChild(Spacer));
        if (HS) HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    // ── 3 個圓形按鈕 ─────────────────────────────────────────────────────
    struct { const TCHAR* Label; void(UPlayerPanelWidget::*Fn)(); } Circles[] = {
        { TEXT("引導"), &UPlayerPanelWidget::OnGuideClicked      },
        { TEXT("圖鑑"), &UPlayerPanelWidget::OnCompendiumClicked },
        { TEXT("設定"), &UPlayerPanelWidget::OnSettingsClicked   },
    };
    for (auto& C : Circles)
    {
        auto* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        FButtonStyle BS = Btn->GetStyle();
        BS.Normal.DrawAs  = ESlateBrushDrawType::Box;
        BS.Normal.TintColor = FSlateColor(kCircleBg);
        BS.Hovered.DrawAs = ESlateBrushDrawType::Box;
        BS.Hovered.TintColor = FSlateColor(FLinearColor(0.30f, 0.52f, 0.90f));
        BS.Pressed.DrawAs = ESlateBrushDrawType::Box;
        BS.Pressed.TintColor = FSlateColor(FLinearColor(0.15f, 0.30f, 0.60f));
        Btn->SetStyle(BS);
        Btn->OnClicked.AddDynamic(this, C.Fn);

        auto* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Lbl->SetText(FText::FromString(C.Label));
        Lbl->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Lbl->SetJustification(ETextJustify::Center);
        FSlateFontInfo F = Lbl->GetFont(); F.Size = 14; Lbl->SetFont(F);
        Btn->SetContent(Lbl);

        // USizeBox 固定 64×64，內包 Border + Button
        auto* Frame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Frame->SetWidthOverride(64.f);
        Frame->SetHeightOverride(64.f);
        auto* FrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        FrameBorder->SetBrushColor(kCircleBg);
        FrameBorder->SetPadding(FMargin(0.f));
        FrameBorder->SetContent(Btn);
        Frame->SetContent(FrameBorder);
        {
            auto* HS = Cast<UHorizontalBoxSlot>(HBox->AddChild(Frame));
            if (HS) { HS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); HS->SetPadding(FMargin(4.f, 4.f)); HS->SetVerticalAlignment(VAlign_Center); }
        }
    }
}

// ── 子頁頁首（← 返回 + 標題） ────────────────────────────────────────────

void UPlayerPanelWidget::BuildPageHeader(UVerticalBox* Root)
{
    auto* PHBox_SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    PHBox_SizeBox->SetMinDesiredHeight(72.f);
    PHBox_SizeBox->SetVisibility(ESlateVisibility::Collapsed);
    {
        auto* S = Cast<UVerticalBoxSlot>(Root->AddChild(PHBox_SizeBox));
        if (S) { S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); S->SetHorizontalAlignment(HAlign_Fill); }
    }
    auto* HdrBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    HdrBorder->SetBrushColor(kHeaderBg);
    HdrBorder->SetPadding(FMargin(12.f, 0.f));
    PHBox_SizeBox->SetContent(HdrBorder);
    PageHeaderBox = PHBox_SizeBox;

    auto* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    HdrBorder->SetContent(HBox);
    PageHeader = HBox;

    // 返回按鈕
    BackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    FButtonStyle BS = BackButton->GetStyle();
    BS.Normal.DrawAs  = ESlateBrushDrawType::NoDrawType;
    BS.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
    BS.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
    BackButton->SetStyle(BS);
    BackButton->OnClicked.AddDynamic(this, &UPlayerPanelWidget::OnBackClicked);
    {
        auto* HS = Cast<UHorizontalBoxSlot>(HBox->AddChild(BackButton));
        if (HS) { HS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); HS->SetVerticalAlignment(VAlign_Center); HS->SetPadding(FMargin(8.f, 0.f, 16.f, 0.f)); }
    }
    auto* BackLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    BackLbl->SetText(FText::FromString(TEXT("← 返回")));
    FSlateFontInfo BF = BackLbl->GetFont(); BF.Size = 16; BackLbl->SetFont(BF);
    BackLbl->SetColorAndOpacity(FSlateColor(kTabActive));
    BackButton->SetContent(BackLbl);

    // 標題
    PageTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    PageTitle->SetText(FText::FromString(TEXT("")));
    FSlateFontInfo TF = PageTitle->GetFont(); TF.Size = 18; PageTitle->SetFont(TF);
    PageTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.1f, 0.1f, 0.2f)));
    {
        auto* HS = Cast<UHorizontalBoxSlot>(HBox->AddChild(PageTitle));
        if (HS) { HS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); HS->SetVerticalAlignment(VAlign_Center); }
    }
}

// ── 內容區（7 個頁容器，逐一建立，切換時用 ContentArea->SetContent() 替換） ──

void UPlayerPanelWidget::BuildContentArea(UVerticalBox* Root)
{
    auto* ContentBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    ContentBorder->SetBrushColor(kContentBg);
    ContentBorder->SetPadding(FMargin(0.f));
    {
        auto* S = Cast<UVerticalBoxSlot>(Root->AddChild(ContentBorder));
        if (S) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
    }

    // 建立 7 個空白頁容器（Stage 3 填入真實子 Widget 之前為空白 Border）
    static const TCHAR* PlaceholderTexts[] = {
        TEXT("個人資訊面板"),
        TEXT("職業能力"),
        TEXT("技能創建空間"),
        TEXT("內部空間"),
        TEXT("引導"),
        TEXT("圖鑑"),
        TEXT("設定"),
    };
    for (int32 i = 0; i < 7; ++i)
    {
        auto* Page = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Page->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
        Page->SetPadding(FMargin(0.f));
        PageContainers.Add(Page);
    }

    // 初始顯示 Stats（頁 0）
    ContentBorder->SetContent(PageContainers[0]);

    // 保存 ContentArea 指標供 SwitchPage 使用
    // （利用 PageContainers[0] 的 parent 往上找 ContentBorder）
    // 直接存成成員
    ContentAreaBorder = ContentBorder;
}

void UPlayerPanelWidget::CreateSubWidgets()
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    // 建立各 Tab 的子 Widget，塞進對應的 PageContainer
    // Tab 0: 個人資訊面板
    StatsContent = CreateWidget<UStatsWidget>(PC, UStatsWidget::StaticClass());
    if (StatsContent && PageContainers.IsValidIndex(0))
        PageContainers[0]->SetContent(StatsContent);

    // Tab 1: 職業能力（空白佔位）
    OccupationContent = CreateWidget<UOccupationWidget>(PC, UOccupationWidget::StaticClass());
    if (OccupationContent && PageContainers.IsValidIndex(1))
        PageContainers[1]->SetContent(OccupationContent);

    // Tab 2: 技能創建空間
    SpellListContent = CreateWidget<USpellListWidget>(PC, USpellListWidget::StaticClass());
    if (SpellListContent && PageContainers.IsValidIndex(2))
        PageContainers[2]->SetContent(SpellListContent);

    // Tab 3: 內部空間（空白佔位）
    InnerWorldContent = CreateWidget<UInnerWorldWidget>(PC, UInnerWorldWidget::StaticClass());
    if (InnerWorldContent && PageContainers.IsValidIndex(3))
        PageContainers[3]->SetContent(InnerWorldContent);

    // 引導 / 圖鑑 / 設定（全版頁面）
    GuideContent = CreateWidget<UGuideWidget>(PC, UGuideWidget::StaticClass());
    if (GuideContent && PageContainers.IsValidIndex(4))
        PageContainers[4]->SetContent(GuideContent);

    CompendiumContent = CreateWidget<UCompendiumWidget>(PC, UCompendiumWidget::StaticClass());
    if (CompendiumContent && PageContainers.IsValidIndex(5))
        PageContainers[5]->SetContent(CompendiumContent);

    SettingsContent = CreateWidget<UPlayerSettingsWidget>(PC, UPlayerSettingsWidget::StaticClass());
    if (SettingsContent && PageContainers.IsValidIndex(6))
        PageContainers[6]->SetContent(SettingsContent);
}

// ── 頁面切換 ──────────────────────────────────────────────────────────────

void UPlayerPanelWidget::SwitchPage(EPage Page)
{
    CurrentPage = Page;

    // 是 Tab 頁（0~3）還是子頁（4~6）
    const bool bIsTabPage = (int32)Page <= 3;
    if (bIsTabPage) LastTabPage = Page;

    // 切換 Header
    if (NormalHeaderBox)
        NormalHeaderBox->SetVisibility(bIsTabPage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (PageHeaderBox)
        PageHeaderBox->SetVisibility(bIsTabPage ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

    // 子頁標題
    if (!bIsTabPage && PageTitle)
    {
        static const TCHAR* SubTitles[] = { TEXT("引導"), TEXT("圖鑑"), TEXT("設定") };
        PageTitle->SetText(FText::FromString(SubTitles[(int32)Page - 4]));
    }

    // 切換內容
    if (ContentAreaBorder && PageContainers.IsValidIndex((int32)Page))
        ContentAreaBorder->SetContent(PageContainers[(int32)Page]);

    // 切換到技能創建空間時刷新圓球列表
    if (Page == EPage::SpellEditor && SpellListContent)
        SpellListContent->RefreshSpellList();

    // 更新 Tab 底線高亮
    UpdateTabHighlight();
}

void UPlayerPanelWidget::UpdateTabHighlight()
{
    const bool bIsTabPage = (int32)CurrentPage <= 3;
    for (int32 i = 0; i < TabButtons.Num() && i < TabUnderlines.Num(); ++i)
    {
        const bool bActive = bIsTabPage && (i == (int32)CurrentPage);
        if (auto* Lbl = Cast<UTextBlock>(TabButtons[i]->GetContent()))
            Lbl->SetColorAndOpacity(FSlateColor(bActive ? kTabActive : kTabInactive));
        TabUnderlines[i]->SetBrushColor(bActive ? kUnderlineOn : kUnderlineOff);
    }
}

// ── 資料刷新（Stage 3 接上 UStatsWidget 後才有作用） ─────────────────────

void UPlayerPanelWidget::RefreshStatsTab(const FCharacterStats& Stats,
    float Hp, float MaxHp, float Mp, float MaxMp,
    int32 Level, const FString& TierName, const UEquipmentComponent* Equip)
{
    if (StatsContent)
        StatsContent->Refresh(Stats, Hp, MaxHp, Mp, MaxMp, Level, TierName, Equip);
}

// ── UFUNCTION 按鈕回呼 ───────────────────────────────────────────────────

void UPlayerPanelWidget::OnTabStats()        { SwitchPage(EPage::Stats);      }
void UPlayerPanelWidget::OnTabOccupation()   { SwitchPage(EPage::Occupation); }
void UPlayerPanelWidget::OnTabSpellEditor()  { SwitchPage(EPage::SpellEditor);}
void UPlayerPanelWidget::OnTabInnerWorld()   { SwitchPage(EPage::InnerWorld); }
void UPlayerPanelWidget::OnGuideClicked()    { SwitchPage(EPage::Guide);      }
void UPlayerPanelWidget::OnCompendiumClicked(){ SwitchPage(EPage::Compendium);}
void UPlayerPanelWidget::OnSettingsClicked() { SwitchPage(EPage::Settings);   }
void UPlayerPanelWidget::OnBackClicked()     { SwitchPage(LastTabPage);       }
