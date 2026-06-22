#include "UGameFlowWidget.h"
#include "UFlowCardWidget.h"
#include "UPlayerCreateWidget.h"
#include "FlowSaveSystem.h"
#include "WorldSaveData.h"
#include "TileWorld3D.h"
#include "MapGenerator3D.h"
#include "UGameSessionSubsystem.h"
#include "ASkillCreatorGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/ScrollBox.h"
#include "Components/ProgressBar.h"

// 定義在這裡（不是標頭檔 inline）：這個翻譯單元已經 #include "TileWorld3D.h" /
// "MapGenerator3D.h"，delete 完整型別的指標才合法。
UGameFlowWidget::~UGameFlowWidget()
{
    delete PendingGenWorld;
    delete PendingGenGen;
}

// ── 內部轉換 ──────────────────────────────────────────────────────────────

static FWorldSaveInfo ToInfo(const FWorldSaveData& D)
{
    FWorldSaveInfo I;
    I.Id       = D.Id;
    I.Name     = D.Name;
    I.Seed     = D.Seed;
    I.WorldDir = D.WorldDir;
    return I;
}

static FCharacterSummaryInfo ToSummary(const FCharacterSaveData& D)
{
    FCharacterSummaryInfo I;
    I.Id    = D.Id;
    I.Name  = D.CharacterName;
    I.Level = D.Level;
    return I;
}

// 統一的標題文字 helper（對應 Godot MakeLabel，GameFlowUI.cs:63-70）
static UTextBlock* MakeTitleLabel(UWidgetTree* Tree, const FString& Text, int32 FontSize, const FLinearColor& Color)
{
    UTextBlock* Lbl = Tree->ConstructWidget<UTextBlock>();
    Lbl->SetText(FText::FromString(Text));
    FSlateFontInfo F = Lbl->GetFont();
    F.Size = FontSize;
    Lbl->SetFont(F);
    Lbl->SetColorAndOpacity(FSlateColor(Color));
    return Lbl;
}

// ── Widget 樹建構 ─────────────────────────────────────────────────────────
//
// 對齊 Godot GameFlowUI 的 5 畫面（GameFlowUI.cs:19-24）：標題 / 角色選擇 / 角色創建 /
// 世界選擇 / 世界創建，疊在同一個 UOverlay，靠 ShowScreen() 切換可見性（對應 Godot ShowScreen，
// GameFlowUI.cs:90-97）。版面用 UMG 自動排版容器（VerticalBox/HorizontalBox/ScrollBox）而非
// Godot 原本的 800×600 絕對座標——GameFlowWidget 先前已因「子項用固定尺寸、外層又設 Fill」
// 撞過一次 0×0 collapse bug（見 commit ce56c44），這裡延續同一套已驗證安全的排版慣例。

void UGameFlowWidget::BuildLayout()
{
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 1.f));
    WidgetTree->RootWidget = Background;

    UOverlay* ScreenStack = WidgetTree->ConstructWidget<UOverlay>();
    Background->SetContent(ScreenStack);

    TitleScreen       = BuildTitleScreen();
    CharSelectScreen  = BuildCharSelectScreen();
    WorldSelectScreen = BuildWorldSelectScreen();
    WorldCreateScreen = BuildWorldCreateScreen();

    // W-10：角色創建精靈（取代原本一頁式 CharCreateScreen）。跟其他畫面一樣 eager 建立、
    // 加進同一個 ScreenStack，靠 ShowScreen() 切換可見性。
    PlayerCreateScreen = CreateWidget<UPlayerCreateWidget>(GetOwningPlayer(), UPlayerCreateWidget::StaticClass());
    TWeakObjectPtr<UGameFlowWidget> WeakThis(this);
    PlayerCreateScreen->OnFinished = [WeakThis](const FCharacterSaveData& Data)
    {
        UGameFlowWidget* Self = WeakThis.Get();
        if (!Self) return;
        FCharacterSaveData Saved;
        FFlowSaveSystem::CreateNewCharacter(Data, Saved);
        Self->RefreshCharacterList();
        Self->ShowScreen(Self->CharSelectScreen);
    };
    PlayerCreateScreen->OnCancelled = [WeakThis]()
    {
        UGameFlowWidget* Self = WeakThis.Get();
        if (!Self) return;
        Self->RefreshCharacterList();
        Self->ShowScreen(Self->CharSelectScreen);
    };

    UWidget* Screens[] = { TitleScreen, CharSelectScreen, PlayerCreateScreen, WorldSelectScreen, WorldCreateScreen };
    for (UWidget* Screen : Screens)
    {
        if (UOverlaySlot* S = ScreenStack->AddChildToOverlay(Screen))
        {
            S->SetHorizontalAlignment(HAlign_Fill);
            S->SetVerticalAlignment(VAlign_Fill);
        }
    }

    ShowScreen(TitleScreen);

    // ── 世界生成 loading 遮罩（對應 Godot _genLoadingOverlay，GameFlowUI.cs:464-480）──
    UBorder* LoadingBg = WidgetTree->ConstructWidget<UBorder>();
    LoadingBg->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.82f));

    UVerticalBox* LoadingStack = WidgetTree->ConstructWidget<UVerticalBox>();
    if (UBorderSlot* S = Cast<UBorderSlot>(LoadingBg->SetContent(LoadingStack)))
    {
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetVerticalAlignment(VAlign_Center);
    }

    WorldGenLoadingLabel = MakeTitleLabel(WidgetTree, TEXT("正在生成世界…"), 28, FLinearColor(0.80f, 0.95f, 0.68f));
    WorldGenLoadingLabel->SetJustification(ETextJustify::Center);
    LoadingStack->AddChildToVerticalBox(WorldGenLoadingLabel);

    UTextBlock* GenSubLbl = WidgetTree->ConstructWidget<UTextBlock>();
    GenSubLbl->SetText(FText::FromString(TEXT("請稍候，正在為您的世界生成地形…")));
    GenSubLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.60f, 0.60f, 0.60f)));
    GenSubLbl->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* S = LoadingStack->AddChildToVerticalBox(GenSubLbl))
        S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

    // 進度條（使用者要求「請稍候」那行底下要有，之前漏放）。寬度用 SizeBox 固定，
    // 否則 ProgressBar 在 VerticalBox 裡預設只會撐到文字寬度，看起來像一條細線。
    UProgressBar* GenProgressBar = WidgetTree->ConstructWidget<UProgressBar>();
    GenProgressBar->SetPercent(0.f);
    GenProgressBar->SetFillColorAndOpacity(FLinearColor(0.45f, 0.75f, 0.95f));
    USizeBox* ProgressBarBox = WidgetTree->ConstructWidget<USizeBox>();
    ProgressBarBox->SetWidthOverride(420.f);
    ProgressBarBox->SetHeightOverride(18.f);
    ProgressBarBox->AddChild(GenProgressBar);
    if (UVerticalBoxSlot* S = LoadingStack->AddChildToVerticalBox(ProgressBarBox))
    {
        S->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
        S->SetHorizontalAlignment(HAlign_Center);
    }
    WorldGenProgressBar = GenProgressBar;

    WorldGenLoadingOverlay = LoadingBg;
    if (UOverlaySlot* S = ScreenStack->AddChildToOverlay(WorldGenLoadingOverlay))
    {
        S->SetHorizontalAlignment(HAlign_Fill);
        S->SetVerticalAlignment(VAlign_Fill);
    }
    WorldGenLoadingOverlay->SetVisibility(ESlateVisibility::Collapsed);

    // ── 確認刪除彈窗（對應 Godot BuildConfirmDialog，GameFlowUI.cs:101-144）────────
    UBorder* ConfirmBg = WidgetTree->ConstructWidget<UBorder>();
    ConfirmBg->SetBrushColor(FLinearColor(0.13f, 0.13f, 0.18f, 1.f));

    UVerticalBox* ConfirmBox = WidgetTree->ConstructWidget<UVerticalBox>();
    ConfirmBg->SetContent(ConfirmBox);

    ConfirmMsgText = WidgetTree->ConstructWidget<UTextBlock>();
    ConfirmMsgText->SetText(FText::FromString(TEXT("確定要刪除？")));
    if (UVerticalBoxSlot* S = ConfirmBox->AddChildToVerticalBox(ConfirmMsgText))
        S->SetPadding(FMargin(20.f, 20.f, 20.f, 4.f));

    // 對應 Godot subMsg「此操作無法復原。」（GameFlowUI.cs:126-128）
    UTextBlock* ConfirmSubText = WidgetTree->ConstructWidget<UTextBlock>();
    ConfirmSubText->SetText(FText::FromString(TEXT("此操作無法復原。")));
    ConfirmSubText->SetColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.40f, 0.40f)));
    if (UVerticalBoxSlot* S = ConfirmBox->AddChildToVerticalBox(ConfirmSubText))
        S->SetPadding(FMargin(20.f, 0.f, 20.f, 16.f));

    UHorizontalBox* ConfirmBtns = WidgetTree->ConstructWidget<UHorizontalBox>();
    if (UVerticalBoxSlot* S = ConfirmBox->AddChildToVerticalBox(ConfirmBtns))
        S->SetPadding(FMargin(20.f, 0.f, 20.f, 20.f));

    UButton* CancelBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* CancelTxt = WidgetTree->ConstructWidget<UTextBlock>();
    CancelTxt->SetText(FText::FromString(TEXT("取消")));
    CancelBtn->AddChild(CancelTxt);
    CancelBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnConfirmDeleteNo);
    if (UHorizontalBoxSlot* S = ConfirmBtns->AddChildToHorizontalBox(CancelBtn))
        S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

    UButton* OkBtn = WidgetTree->ConstructWidget<UButton>();
    OkBtn->SetBackgroundColor(FLinearColor(0.50f, 0.12f, 0.12f, 1.f));
    UTextBlock* OkTxt = WidgetTree->ConstructWidget<UTextBlock>();
    OkTxt->SetText(FText::FromString(TEXT("確定刪除")));
    OkBtn->AddChild(OkTxt);
    OkBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnConfirmDeleteYes);
    ConfirmBtns->AddChildToHorizontalBox(OkBtn);

    ConfirmOverlay = ConfirmBg;
    if (UOverlaySlot* S = ScreenStack->AddChildToOverlay(ConfirmOverlay))
    {
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetVerticalAlignment(VAlign_Center);
    }
    ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
}

// ── 標題畫面（對應 Godot BuildTitleScreen，GameFlowUI.cs:155-177）─────────────────

UWidget* UGameFlowWidget::BuildTitleScreen()
{
    UBorder* Bg = WidgetTree->ConstructWidget<UBorder>();
    Bg->SetBrushColor(FLinearColor(0.07f, 0.07f, 0.11f, 1.f));

    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>();
    if (UBorderSlot* S = Cast<UBorderSlot>(Bg->SetContent(Stack)))
    {
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetVerticalAlignment(VAlign_Center);
    }

    UTextBlock* Title = MakeTitleLabel(WidgetTree, TEXT("SkillCreator"), 64, FLinearColor(0.92f, 0.86f, 0.55f));
    Title->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* S = Stack->AddChildToVerticalBox(Title))
        S->SetHorizontalAlignment(HAlign_Center);

    USizeBox* Spacer = WidgetTree->ConstructWidget<USizeBox>();
    Spacer->SetHeightOverride(48.f);
    Stack->AddChildToVerticalBox(Spacer);

    UButton* EnterBtn = WidgetTree->ConstructWidget<UButton>();
    EnterBtn->SetBackgroundColor(FLinearColor(0.18f, 0.35f, 0.18f, 1.f));
    UTextBlock* EnterTxt = MakeTitleLabel(WidgetTree, TEXT("進入遊戲"), 20, FLinearColor::White);
    EnterTxt->SetJustification(ETextJustify::Center);
    EnterBtn->AddChild(EnterTxt);
    EnterBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnTitleEnterClicked);

    USizeBox* BtnSize = WidgetTree->ConstructWidget<USizeBox>();
    BtnSize->SetWidthOverride(200.f);
    BtnSize->SetHeightOverride(52.f);
    BtnSize->SetContent(EnterBtn);
    if (UVerticalBoxSlot* S = Stack->AddChildToVerticalBox(BtnSize))
        S->SetHorizontalAlignment(HAlign_Center);

    return Bg;
}

// ── 角色選擇畫面（對應 Godot BuildCharSelectScreen，GameFlowUI.cs:181-202）──────────

UWidget* UGameFlowWidget::BuildCharSelectScreen()
{
    UBorder* Bg = WidgetTree->ConstructWidget<UBorder>();
    Bg->SetBrushColor(FLinearColor(0.09f, 0.09f, 0.14f, 1.f));

    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
    Bg->SetContent(Root);

    UTextBlock* Title = MakeTitleLabel(WidgetTree, TEXT("─── 選擇角色 ───"), 30, FLinearColor(0.92f, 0.86f, 0.55f));
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Title))
        S->SetPadding(FMargin(20.f, 16.f, 0.f, 8.f));

    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Scroll))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetPadding(FMargin(20.f, 0.f, 20.f, 0.f));
    }

    CharListContainer = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(CharListContainer);

    // 「創建角色」按鈕（對應 Godot createBtn，GameFlowUI.cs:198-201，左下角）
    UButton* CreateBtn = WidgetTree->ConstructWidget<UButton>();
    CreateBtn->SetBackgroundColor(FLinearColor(0.22f, 0.22f, 0.32f, 1.f));
    UTextBlock* CreateTxt = WidgetTree->ConstructWidget<UTextBlock>();
    CreateTxt->SetText(FText::FromString(TEXT("創建角色")));
    CreateBtn->AddChild(CreateTxt);
    CreateBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnCharCreateNavClicked);

    USizeBox* BtnSize = WidgetTree->ConstructWidget<USizeBox>();
    BtnSize->SetWidthOverride(140.f);
    BtnSize->SetHeightOverride(40.f);
    BtnSize->SetContent(CreateBtn);
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(BtnSize))
    {
        S->SetHorizontalAlignment(HAlign_Left);
        S->SetPadding(FMargin(20.f, 8.f, 0.f, 16.f));
    }

    return Bg;
}

// ── 世界選擇畫面（對應 Godot BuildWorldSelectScreen，GameFlowUI.cs:307-338）─────────

UWidget* UGameFlowWidget::BuildWorldSelectScreen()
{
    UBorder* Bg = WidgetTree->ConstructWidget<UBorder>();
    Bg->SetBrushColor(FLinearColor(0.07f, 0.11f, 0.09f, 1.f));

    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
    Bg->SetContent(Root);

    UTextBlock* Title = MakeTitleLabel(WidgetTree, TEXT("─── 選擇世界 ───"), 30, FLinearColor(0.80f, 0.95f, 0.68f));
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Title))
        S->SetPadding(FMargin(20.f, 16.f, 0.f, 8.f));

    // 返回按鈕（對應 Godot backBtn，GameFlowUI.cs:317-324，回角色選擇 + 重建角色清單）
    UButton* BackBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* BackTxt = WidgetTree->ConstructWidget<UTextBlock>();
    BackTxt->SetText(FText::FromString(TEXT("← 返回角色選擇")));
    BackBtn->AddChild(BackTxt);
    BackBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnWorldSelectBackClicked);
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(BackBtn))
    {
        S->SetHorizontalAlignment(HAlign_Left);
        S->SetPadding(FMargin(20.f, 0.f, 0.f, 8.f));
    }

    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Scroll))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetPadding(FMargin(20.f, 0.f, 20.f, 0.f));
    }

    WorldListContainer = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(WorldListContainer);

    // 「創建世界」按鈕（對應 Godot createBtn，GameFlowUI.cs:334-337，左下角）
    UButton* CreateBtn = WidgetTree->ConstructWidget<UButton>();
    CreateBtn->SetBackgroundColor(FLinearColor(0.22f, 0.22f, 0.32f, 1.f));
    UTextBlock* CreateTxt = WidgetTree->ConstructWidget<UTextBlock>();
    CreateTxt->SetText(FText::FromString(TEXT("創建世界")));
    CreateBtn->AddChild(CreateTxt);
    CreateBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnWorldCreateNavClicked);

    USizeBox* BtnSize = WidgetTree->ConstructWidget<USizeBox>();
    BtnSize->SetWidthOverride(140.f);
    BtnSize->SetHeightOverride(40.f);
    BtnSize->SetContent(CreateBtn);
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(BtnSize))
    {
        S->SetHorizontalAlignment(HAlign_Left);
        S->SetPadding(FMargin(20.f, 8.f, 0.f, 16.f));
    }

    return Bg;
}

// ── 世界創建畫面（對應 Godot BuildWorldCreateScreen，GameFlowUI.cs:403-481）─────────

UWidget* UGameFlowWidget::BuildWorldCreateScreen()
{
    UBorder* Bg = WidgetTree->ConstructWidget<UBorder>();
    Bg->SetBrushColor(FLinearColor(0.07f, 0.11f, 0.09f, 1.f));

    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
    Bg->SetContent(Root);

    UTextBlock* Title = MakeTitleLabel(WidgetTree, TEXT("─── 創建世界 ───"), 30, FLinearColor(0.80f, 0.95f, 0.68f));
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Title))
        S->SetPadding(FMargin(20.f, 16.f, 0.f, 8.f));

    UButton* BackBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* BackTxt = WidgetTree->ConstructWidget<UTextBlock>();
    BackTxt->SetText(FText::FromString(TEXT("← 返回")));
    BackBtn->AddChild(BackTxt);
    BackBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnWorldCreateBackClicked);
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(BackBtn))
    {
        S->SetHorizontalAlignment(HAlign_Left);
        S->SetPadding(FMargin(20.f, 0.f, 0.f, 24.f));
    }

    UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(NameRow))
        S->SetPadding(FMargin(20.f, 0.f, 20.f, 0.f));

    UTextBlock* NameLbl = WidgetTree->ConstructWidget<UTextBlock>();
    NameLbl->SetText(FText::FromString(TEXT("世界名稱：")));
    if (UHorizontalBoxSlot* S = NameRow->AddChildToHorizontalBox(NameLbl))
        S->SetVerticalAlignment(VAlign_Center);

    WorldNameInput = WidgetTree->ConstructWidget<UEditableText>();
    WorldNameInput->SetHintText(FText::FromString(TEXT("新世界")));
    if (UHorizontalBoxSlot* S = NameRow->AddChildToHorizontalBox(WorldNameInput))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    // 彈性空白：把下面的確認鈕推到畫面最底，理由同 BuildCharCreateScreen()
    USpacer* FillSpacer = WidgetTree->ConstructWidget<USpacer>();
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(FillSpacer))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    // 確認創建世界（對應 Godot confirmBtn，GameFlowUI.cs:429-462，含 loading 遮罩 + 預生成）
    UButton* ConfirmBtn = WidgetTree->ConstructWidget<UButton>();
    ConfirmBtn->SetBackgroundColor(FLinearColor(0.18f, 0.35f, 0.18f, 1.f));
    UTextBlock* ConfirmTxt = WidgetTree->ConstructWidget<UTextBlock>();
    ConfirmTxt->SetText(FText::FromString(TEXT("確認創建世界")));
    ConfirmBtn->AddChild(ConfirmTxt);
    ConfirmBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnWorldCreateConfirmClicked);

    USizeBox* BtnSize = WidgetTree->ConstructWidget<USizeBox>();
    BtnSize->SetWidthOverride(160.f);
    BtnSize->SetHeightOverride(44.f);
    BtnSize->SetContent(ConfirmBtn);
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(BtnSize))
    {
        S->SetHorizontalAlignment(HAlign_Right);
        S->SetPadding(FMargin(0.f, 0.f, 20.f, 0.f));
    }

    return Bg;
}

void UGameFlowWidget::ShowScreen(UWidget* Target)
{
    UWidget* Screens[] = { TitleScreen, CharSelectScreen, PlayerCreateScreen, WorldSelectScreen, WorldCreateScreen };
    for (UWidget* Screen : Screens)
        if (Screen)
            Screen->SetVisibility(Screen == Target ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

// ── UUserWidget overrides ─────────────────────────────────────────────────
//
// 用 NativeOnInitialized()，不是 NativeConstruct()：UWidget::TakeWidget_Private()
// 第一次呼叫時，會先呼叫 RebuildWidget()（讀取 WidgetTree->RootWidget，當時若是
// null 就回退成空的 SNew(SSpacer)，見 UserWidget.cpp:1203），事後才呼叫
// OnWidgetRebuilt() → NativeConstruct()——這代表「在 NativeConstruct() 裡才設定
// RootWidget」對第一次 AddToViewport() 必定太晚，整個畫面會是空的，且 MyWidget
// 快取後不會再重新 take，永遠卡死在空畫面。NativeOnInitialized() 是在
// UUserWidget::Initialize() 裡呼叫（CreateWidget() 當下就執行，早於
// AddToViewport()），確認 CreateWidgetInstance(APlayerController&,...) 會先
// SetPlayerContext() 才 Initialize()，所以 PlayerContext.IsValid() 一定為真，
// NativeOnInitialized() 保證會在這裡觸發。
void UGameFlowWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();
}

void UGameFlowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (bIsGeneratingWorld)
        PollWorldGeneration();
}

// ── BlueprintNativeEvent 預設實作 ─────────────────────────────────────────
//
// 清單卡片用 UFlowCardWidget（對應 Godot MakeCharCard/MakeWorldCard 整塊可點擊卡片 +
// 獨立 🗑 刪除鈕，GameFlowUI.cs:220-253 / 356-395），取代先前「文字列 + 手動輸入 ID」的暫代版。

void UGameFlowWidget::OnWorldListRefreshed_Implementation(const TArray<FWorldSaveInfo>& Worlds)
{
    if (!WorldListContainer) return;
    WorldListContainer->ClearChildren();

    if (Worlds.IsEmpty())
    {
        UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>();
        Empty->SetText(FText::FromString(TEXT("尚無世界，請點擊下方「創建世界」。")));
        Empty->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)));
        WorldListContainer->AddChildToVerticalBox(Empty);
        return;
    }

    for (const FWorldSaveInfo& W : Worlds)
    {
        UFlowCardWidget* Card = CreateWidget<UFlowCardWidget>(this, UFlowCardWidget::StaticClass());
        const FString Name = W.Name;
        Card->Setup(W.Id, FText::FromString(FString::Printf(TEXT("  %s"), *W.Name)),
            FLinearColor(0.16f, 0.26f, 0.20f, 1.f),
            [this](const FString& InId) { EnterWorld(InId); },
            [this, Name](const FString& InId) { ShowConfirmDelete(InId, Name, /*bIsChar=*/false); });
        if (UVerticalBoxSlot* S = WorldListContainer->AddChildToVerticalBox(Card))
        {
            S->SetHorizontalAlignment(HAlign_Fill);
            S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
        }
    }
}

void UGameFlowWidget::OnCharacterListRefreshed_Implementation(const TArray<FCharacterSummaryInfo>& Characters)
{
    if (!CharListContainer) return;
    CharListContainer->ClearChildren();

    if (Characters.IsEmpty())
    {
        UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>();
        Empty->SetText(FText::FromString(TEXT("尚無角色，請點擊下方「創建角色」。")));
        Empty->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)));
        CharListContainer->AddChildToVerticalBox(Empty);
        return;
    }

    for (const FCharacterSummaryInfo& C : Characters)
    {
        UFlowCardWidget* Card = CreateWidget<UFlowCardWidget>(this, UFlowCardWidget::StaticClass());
        const FString Name = C.Name;
        Card->Setup(C.Id, FText::FromString(FString::Printf(TEXT("  %s    LV %d"), *C.Name, C.Level)),
            FLinearColor(0.16f, 0.20f, 0.28f, 1.f),
            [this](const FString& InId) { SelectCharacter(InId); },
            [this, Name](const FString& InId) { ShowConfirmDelete(InId, Name, /*bIsChar=*/true); });
        if (UVerticalBoxSlot* S = CharListContainer->AddChildToVerticalBox(Card))
        {
            S->SetHorizontalAlignment(HAlign_Fill);
            S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
        }
    }
}

void UGameFlowWidget::OnEnterGame_Implementation(const FWorldSaveInfo& SelectedWorld)
{
    // 單一地圖（MainMap）兼當選單與遊戲關卡，不需要 OpenLevel 換關卡；
    // EnterWorld() 已經把選好的 FWorldSaveData + SelectedCharacter 寫進
    // UGameSessionSubsystem，這裡直接讀出來交給 GameMode。
    UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
    UGameSessionSubsystem* Sub = GI ? GI->GetSubsystem<UGameSessionSubsystem>() : nullptr;
    if (Sub && Sub->HasPendingWorld() && Sub->HasPendingCharacter())
    {
        if (ASkillCreatorGameMode* GM = Cast<ASkillCreatorGameMode>(UGameplayStatics::GetGameMode(this)))
            GM->StartGameplayWithWorld(Sub->GetPendingWorld(), Sub->GetPendingCharacter());
    }
}

// ── 按鈕回呼 ───────────────────────────────────────────────────────────────

void UGameFlowWidget::OnTitleEnterClicked()
{
    // 對應 Godot 標題畫面 btn.Pressed（GameFlowUI.cs:171-176）
    RefreshCharacterList();
    ShowScreen(CharSelectScreen);
}

void UGameFlowWidget::OnCharCreateNavClicked()
{
    if (PlayerCreateScreen) PlayerCreateScreen->ResetAndShowFirstStep();
    ShowScreen(PlayerCreateScreen);
}

void UGameFlowWidget::OnWorldSelectBackClicked()
{
    // 對應 Godot backBtn.Pressed（GameFlowUI.cs:319-323）
    RefreshCharacterList();
    ShowScreen(CharSelectScreen);
}

void UGameFlowWidget::OnWorldCreateNavClicked()
{
    ShowScreen(WorldCreateScreen);
}

void UGameFlowWidget::OnWorldCreateBackClicked()
{
    ShowScreen(WorldSelectScreen);
}

void UGameFlowWidget::OnWorldCreateConfirmClicked()
{
    // 對應 Godot confirmBtn.Pressed（GameFlowUI.cs:431-461）：空白名稱預設「新世界」。
    // 2026-06-21 改為逐 tick 輪詢（StartWorldGeneration/PollWorldGeneration/
    // FinishWorldGeneration），不再同步 busy-wait——原本的寫法會讓整個視窗在生成期間
    // 完全沒有訊息迴圈可跑，使用者實測回報「畫面卡住，搞不清楚是真的在算還是死掉」，
    // 詳見 docs/開發血汗錄.md 第 2 案。
    FString Name = WorldNameInput ? WorldNameInput->GetText().ToString().TrimStartAndEnd() : FString();
    if (Name.IsEmpty())
        Name = TEXT("新世界");

    if (WorldGenLoadingLabel)
        WorldGenLoadingLabel->SetText(FText::FromString(FString::Printf(TEXT("正在生成世界「%s」…"), *Name)));
    if (WorldGenLoadingOverlay)
        WorldGenLoadingOverlay->SetVisibility(ESlateVisibility::Visible);

    StartWorldGeneration(Name, FMath::RandRange(1000, 99999));
}

void UGameFlowWidget::StartWorldGeneration(const FString& Name, int32 Seed)
{
    FWorldSaveData NewWorld;
    if (!FFlowSaveSystem::CreateNewWorld(Name, Seed, NewWorld))
    {
        if (WorldGenLoadingOverlay) WorldGenLoadingOverlay->SetVisibility(ESlateVisibility::Collapsed);
        if (WorldNameInput) WorldNameInput->SetText(FText::GetEmpty());
        ShowScreen(WorldSelectScreen);
        return;
    }

    // 同 CreateWorld()：此時玩家還在選單流程，沒有 AVoxelWorldActor，用獨立 standalone 物件
    PendingGenWorld = new FTileWorld3D();
    PendingGenWorld->Width     = 0;
    PendingGenWorld->Height    = 256; // 對應 AVoxelWorldActor::WorldHeight 預設值
    PendingGenWorld->Depth     = 0;
    PendingGenWorld->WorldSeed = NewWorld.Seed;

    PendingGenGen = new FMapGenerator3D();
    PendingGenGen->InitTerrainParams(PendingGenWorld->Width, PendingGenWorld->Height,
                                      PendingGenWorld->Depth, PendingGenWorld->WorldSeed);

    PendingGenMeta = NewWorld;

    FFlowSaveSystem::StartPregenerateSpawnArea(*PendingGenWorld, *PendingGenGen, PendingGenMeta);
    PendingGenTotalChunks = PendingGenGen->GetPendingChunkCount();

    if (WorldGenProgressBar) WorldGenProgressBar->SetPercent(0.f);
    bIsGeneratingWorld = true;
}

void UGameFlowWidget::PollWorldGeneration()
{
    if (!PendingGenGen || !PendingGenWorld) return;

    PendingGenGen->ApplyPendingChunks(*PendingGenWorld, /*MaxPerFrame=*/999);

    if (PendingGenTotalChunks > 0)
    {
        const int32 Remaining = PendingGenGen->GetPendingChunkCount();
        const int32 Done      = FMath::Clamp(PendingGenTotalChunks - Remaining, 0, PendingGenTotalChunks);
        if (WorldGenLoadingLabel)
            WorldGenLoadingLabel->SetText(FText::FromString(FString::Printf(
                TEXT("正在生成世界「%s」…（%d / %d 區塊）"),
                *PendingGenMeta.Name, Done, PendingGenTotalChunks)));
        if (WorldGenProgressBar)
            WorldGenProgressBar->SetPercent(static_cast<float>(Done) / static_cast<float>(PendingGenTotalChunks));
    }

    if (!PendingGenGen->HasPendingChunks())
        FinishWorldGeneration();
}

void UGameFlowWidget::FinishWorldGeneration()
{
    bIsGeneratingWorld = false;

    if (PendingGenWorld)
        FFlowSaveSystem::FinishPregenerateSpawnArea(*PendingGenWorld, PendingGenMeta);

    delete PendingGenWorld;
    delete PendingGenGen;
    PendingGenWorld = nullptr;
    PendingGenGen   = nullptr;
    PendingGenTotalChunks = 0;

    RefreshWorldList();

    if (WorldGenLoadingOverlay)
        WorldGenLoadingOverlay->SetVisibility(ESlateVisibility::Collapsed);
    if (WorldNameInput)
        WorldNameInput->SetText(FText::GetEmpty());
    ShowScreen(WorldSelectScreen);
}

void UGameFlowWidget::ShowConfirmDelete(const FString& Id, const FString& Name, bool bIsChar)
{
    // 對應 Godot ShowConfirm（GameFlowUI.cs:146-151），訊息格式對齊 GameFlowUI.cs:249/391
    PendingDeleteId      = Id;
    bPendingDeleteIsChar = bIsChar;
    if (ConfirmMsgText)
    {
        const FString TypeName = bIsChar ? TEXT("角色") : TEXT("世界");
        ConfirmMsgText->SetText(FText::FromString(
            FString::Printf(TEXT("確定要刪除%s「%s」嗎？"), *TypeName, *Name)));
    }
    if (ConfirmOverlay)
        ConfirmOverlay->SetVisibility(ESlateVisibility::Visible);
}

void UGameFlowWidget::OnConfirmDeleteYes()
{
    if (ConfirmOverlay)
        ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);

    if (bPendingDeleteIsChar)
        RemoveCharacter(PendingDeleteId);
    else
        RemoveWorld(PendingDeleteId);

    PendingDeleteId.Empty();
}

void UGameFlowWidget::OnConfirmDeleteNo()
{
    if (ConfirmOverlay)
        ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
    PendingDeleteId.Empty();
}

// ── BlueprintCallable（角色）─────────────────────────────────────────────

void UGameFlowWidget::RefreshCharacterList()
{
    TArray<FCharacterSaveData> All;
    FFlowSaveSystem::ListAllCharacters(All);

    CachedCharacters.Reset(All.Num());
    for (const FCharacterSaveData& D : All)
        CachedCharacters.Add(ToSummary(D));

    OnCharacterListRefreshed(CachedCharacters);
}

bool UGameFlowWidget::CreateCharacter(const FString& Name)
{
    FCharacterSaveData NewChar;
    if (!FFlowSaveSystem::CreateNewCharacter(Name, NewChar)) return false;
    RefreshCharacterList();
    return true;
}

bool UGameFlowWidget::RemoveCharacter(const FString& CharacterId)
{
    if (!FFlowSaveSystem::DeleteCharacter(CharacterId)) return false;
    RefreshCharacterList();
    return true;
}

void UGameFlowWidget::SelectCharacter(const FString& CharacterId)
{
    FCharacterSaveData Loaded;
    if (!FFlowSaveSystem::LoadCharacter(CharacterId, Loaded)) return;

    SelectedCharacter = Loaded;

    if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
        if (UGameSessionSubsystem* Sub = GI->GetSubsystem<UGameSessionSubsystem>())
            Sub->SetPendingCharacter(SelectedCharacter);

    // 對應 Godot clickBtn.Pressed（GameFlowUI.cs:241）：選好角色後切到世界選擇畫面
    RefreshWorldList();
    ShowScreen(WorldSelectScreen);
}

// ── BlueprintCallable（世界）─────────────────────────────────────────────

void UGameFlowWidget::RefreshWorldList()
{
    TArray<FWorldSaveData> All;
    FFlowSaveSystem::ListAllWorlds(All);

    CachedWorlds.Reset(All.Num());
    for (const FWorldSaveData& D : All)
        CachedWorlds.Add(ToInfo(D));

    OnWorldListRefreshed(CachedWorlds);
}

bool UGameFlowWidget::CreateWorld(const FString& Name, int32 Seed)
{
    FWorldSaveData NewWorld;
    if (!FFlowSaveSystem::CreateNewWorld(Name, Seed, NewWorld)) return false;

    // Godot GameFlowUI.cs:436-461（PregenerateWorld）：建立世界時立即預生成出生區地形，
    // 之後永遠走磁碟讀取/懶生成路徑。此時玩家還在選單流程，沒有 AVoxelWorldActor 可用，
    // 用獨立 standalone FTileWorld3D/FMapGenerator3D（與 ASkillCreatorGameMode 首次進入時
    // 共用同一套 FFlowSaveSystem::PregenerateSpawnArea，避免兩處各自維護一份相同邏輯）。
    FTileWorld3D TempWorld;
    TempWorld.Width     = 0;
    TempWorld.Height    = 256; // 對應 AVoxelWorldActor::WorldHeight 預設值
    TempWorld.Depth     = 0;
    TempWorld.WorldSeed = NewWorld.Seed;

    FMapGenerator3D TempGen;
    TempGen.InitTerrainParams(TempWorld.Width, TempWorld.Height, TempWorld.Depth, TempWorld.WorldSeed);

    FFlowSaveSystem::PregenerateSpawnArea(TempWorld, TempGen, NewWorld);

    RefreshWorldList();
    return true;
}

bool UGameFlowWidget::RemoveWorld(const FString& WorldId)
{
    if (!FFlowSaveSystem::DeleteWorld(WorldId)) return false;
    RefreshWorldList();
    return true;
}

void UGameFlowWidget::EnterWorld(const FString& WorldId)
{
    const FWorldSaveInfo* Found = CachedWorlds.FindByPredicate(
        [&WorldId](const FWorldSaveInfo& I){ return I.Id == WorldId; });
    if (!Found) return;

    FWorldSaveData Meta;
    if (!FFlowSaveSystem::LoadWorldMeta(Found->WorldDir, Meta)) return;
    Meta.WorldDir = Found->WorldDir;

    // Godot GameFlowUI.cs:379 — 進入世界時更新最後遊玩時間並落地存檔
    Meta.LastPlayed = FDateTime::Now();
    Meta.SaveMeta(FFlowSaveSystem::MetaPath(Meta.WorldDir));

    if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
        if (UGameSessionSubsystem* Sub = GI->GetSubsystem<UGameSessionSubsystem>())
            Sub->SetPendingWorld(Meta);

    OnEnterGame(*Found);
}
