#include "UBlockEditorWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "UPaletteItemWidget.h"
#include "UBlockDragDropOp.h"
#include "UBlockCardWidget.h"
#include "UBlockTrashZoneWidget.h"
#include "TotemLibrary.h"
#include "FBlockNode.h"
#include "BlockUIDescriptor.h"
#include "SpellArray.h"
#include "SpellSlotSync.h"
#include "AbilityPointCalculator.h"
#include "SpellDescriptionGenerator.h"
#include "ManaTypeRegistry.h"
#include "SafetyGuard.h"
#include "Instruction.h"
#include "SpellGroup.h"
#include "Components/ProgressBar.h"
#include "Components/SpinBox.h"
#include "Components/CheckBox.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "SlateBrushHelpers.h"

// ── 編輯器設定 INI（對應 Godot EditorSettings 持久化）──────────────────────────────────────────

static const TCHAR* BlockEditorCfgSection = TEXT("BlockEditorSettings");

static FString BlockEditorCfgPath()
{
    return FPaths::ProjectUserDir() / TEXT("BlockEditorSettings.ini");
}

// ── Palette 顏色表（對應 Godot AbilityEditorUI.cs:1486-1498 TotemClr / 1528-1542 EngraveClr）──

static FLinearColor TotemTypeColor(ETotemType T)
{
    switch (T)
    {
        case ETotemType::Area:         return FLinearColor(0.55f, 0.80f, 1.00f);
        case ETotemType::Technique:    return FLinearColor(1.00f, 0.72f, 0.35f);
        case ETotemType::Projectile:   return FLinearColor(0.90f, 0.60f, 1.00f);
        case ETotemType::Passive:      return FLinearColor(0.60f, 0.92f, 0.65f);
        case ETotemType::Morph:        return FLinearColor(0.40f, 0.90f, 0.80f);
        case ETotemType::Displacement: return FLinearColor(0.65f, 0.95f, 0.30f);
        case ETotemType::Summon:       return FLinearColor(1.00f, 0.75f, 0.20f);
        case ETotemType::Domain:       return FLinearColor(0.85f, 0.40f, 1.00f);
        default:                       return FLinearColor(0.50f, 0.85f, 0.72f); // Custom
    }
}

static FLinearColor EngraveClrOf(EEngraveColor C)
{
    switch (C)
    {
        case EEngraveColor::Action:    return FLinearColor(0.35f, 0.90f, 0.82f);
        case EEngraveColor::White:     return FLinearColor(0.93f, 0.93f, 0.93f);
        case EEngraveColor::Red:       return FLinearColor(1.00f, 0.38f, 0.38f);
        case EEngraveColor::Green:     return FLinearColor(0.38f, 0.88f, 0.48f);
        case EEngraveColor::Blue:      return FLinearColor(0.38f, 0.60f, 1.00f);
        case EEngraveColor::Yellow:    return FLinearColor(1.00f, 0.88f, 0.28f);
        case EEngraveColor::Orange:    return FLinearColor(1.00f, 0.58f, 0.18f);
        case EEngraveColor::Purple:    return FLinearColor(0.80f, 0.38f, 1.00f);
        case EEngraveColor::Black:     return FLinearColor(0.68f, 0.48f, 0.88f);
        case EEngraveColor::Elemental: return FLinearColor(1.00f, 0.78f, 0.25f);
        case EEngraveColor::Law:       return FLinearColor(0.78f, 0.78f, 0.95f);
        default:                       return FLinearColor::White;
    }
}


void UBlockEditorWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();
}

void UBlockEditorWidget::BuildLayout()
{
    // 根節點直接用 UBorder（不再用 SizeBox + GetViewportSize）。
    // PlayerController 呼叫端已設 SetAnchorsInViewport(0,0,1,1) + SetOffsetsInViewport(0,0,0,0)，
    // viewport slot 等於整個螢幕，UBorder 填滿 slot 即可。
    // Godot AbilityEditorUI.cs:126 ColorRect(0.11,0.11,0.14)
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrush(MakeSolidBrush(FLinearColor(0.11f, 0.11f, 0.14f, 0.98f)));
    WidgetTree->RootWidget = Background;

    // Overlay 讓 Phase 6/7 的確認彈窗能疊在整個編輯器上層
    UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>();
    Background->SetContent(RootOverlay);

    UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>();
    if (UOverlaySlot* S = RootOverlay->AddChildToOverlay(MainVBox))
    {
        S->SetHorizontalAlignment(HAlign_Fill);
        S->SetVerticalAlignment(VAlign_Fill);
    }

    if (UVerticalBoxSlot* HeaderSlot = MainVBox->AddChildToVerticalBox(BuildHeader()))
        HeaderSlot->SetHorizontalAlignment(HAlign_Fill);

    if (UVerticalBoxSlot* BodySlot = MainVBox->AddChildToVerticalBox(BuildBody()))
    {
        BodySlot->SetHorizontalAlignment(HAlign_Fill);
        BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    // 確認彈窗用的全螢幕遮罩，初始隱藏（Phase 6/7 共用 ShowConfirmDialog 顯示）
    ConfirmOverlay = WidgetTree->ConstructWidget<UBorder>();
    ConfirmOverlay->SetBrush(MakeSolidBrush(FLinearColor(0.f, 0.f, 0.f, 0.65f)));
    ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
    if (UOverlaySlot* S = RootOverlay->AddChildToOverlay(ConfirmOverlay))
    {
        S->SetHorizontalAlignment(HAlign_Fill);
        S->SetVerticalAlignment(VAlign_Fill);
    }

    // 編輯器設定彈窗遮罩（Godot AbilityEditorUI.cs:211-216 gearBtn + ShowSettingsPopup）
    LoadEditorSettings();
    EditorSettingsOverlay = WidgetTree->ConstructWidget<UBorder>();
    EditorSettingsOverlay->SetBrush(MakeSolidBrush(FLinearColor(0.f, 0.f, 0.f, 0.65f)));
    EditorSettingsOverlay->SetVisibility(ESlateVisibility::Collapsed);
    if (UOverlaySlot* S = RootOverlay->AddChildToOverlay(EditorSettingsOverlay))
    {
        S->SetHorizontalAlignment(HAlign_Fill);
        S->SetVerticalAlignment(VAlign_Fill);
    }
}

UWidget* UBlockEditorWidget::BuildHeader()
{
    // Godot AbilityEditorUI.cs:151-152：Panel(0.16,0.16,0.20)，固定高度 50px
    USizeBox* HeaderSize = WidgetTree->ConstructWidget<USizeBox>();
    HeaderSize->SetHeightOverride(50.f);

    UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>();
    HeaderBg->SetBrush(MakeSolidBrush(FLinearColor(0.16f, 0.16f, 0.20f, 1.f)));
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
    SpellNameBox->OnTextCommitted.AddDynamic(this, &UBlockEditorWidget::OnSpellNameCommitted);
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
    FlexSpacer->SetBrush(MakeSolidBrush(FLinearColor::Transparent));
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(FlexSpacer))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    // 5 組技能組切換 dot（Godot AbilityEditorUI.cs:192-208，26×26，間隔 3px）
    UHorizontalBox* DotRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(DotRow))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        S->SetVerticalAlignment(VAlign_Center);
    }

    // 2026-06-22 修崩潰：AddDynamic 是巨集，會把第二個參數「原樣字串化」去查 UFunction
    // 名稱（不是真的在執行期接受函式指標變數）。原本寫 DotHandlers[i] 傳一個陣列元素，
    // 巨集字串化後變成字面文字 "DotHandlers[i]"，引擎拿這個字串去驗證函式名稱格式時
    // 直接 assert 崩潰（Delegate.h:474 "'DotHandlers[i]' does not look like a member
    // function"）——必須在呼叫處直接寫 &Class::Method 這種字面運算式，不能用變數轉發。
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
        switch (i)
        {
            case 0: Dot->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnGroupDot0Clicked); break;
            case 1: Dot->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnGroupDot1Clicked); break;
            case 2: Dot->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnGroupDot2Clicked); break;
            case 3: Dot->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnGroupDot3Clicked); break;
            case 4: Dot->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnGroupDot4Clicked); break;
            default: break;
        }
        if (UHorizontalBoxSlot* S = DotRow->AddChildToHorizontalBox(Dot))
        {
            S->SetPadding(FMargin(1.5f));
            S->SetVerticalAlignment(VAlign_Center);
        }
        GroupDots[i] = Dot;
    }

    // 齒輪按鈕（Godot AbilityEditorUI.cs:211-216，30×30，tooltip "編輯器設定"）
    {
        UButton* GearBtn = WidgetTree->ConstructWidget<UButton>();
        GearBtn->SetBackgroundColor(FLinearColor(0.18f, 0.18f, 0.24f, 1.f));
        UTextBlock* GearTxt = WidgetTree->ConstructWidget<UTextBlock>();
        GearTxt->SetText(FText::FromString(TEXT("⚙")));
        GearBtn->AddChild(GearTxt);
        GearBtn->SetToolTipText(FText::FromString(TEXT("編輯器設定")));
        GearBtn->OnClicked.AddLambda([this]() { ShowEditorSettingsPopup(); });
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(GearBtn))
        {
            S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
            S->SetPadding(FMargin(4.f, 0.f));
            S->SetVerticalAlignment(VAlign_Center);
        }
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
    LeftPanel->SetBrush(MakeSolidBrush(FLinearColor(0.13f, 0.13f, 0.17f, 1.f)));
    LeftSize->SetContent(LeftPanel);
    if (UHorizontalBoxSlot* S = BodyRow->AddChildToHorizontalBox(LeftSize))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    BuildPalette();

    // 中央積木卡片清單容器（內容於 Phase 3 填入）+ 右下角垃圾桶疊加層
    // 對應 Godot ScriptCanvas.cs:104-134：_trashZone 是疊在整個畫布 Control 上、錨點
    // 右下角的 Panel，不是卡片清單本身的子節點。這裡用 UOverlay 包住 CenterScroll，
    // 讓垃圾桶懸浮在卡片清單上層且不影響清單的捲動/排版。
    UOverlay* CenterOverlay = WidgetTree->ConstructWidget<UOverlay>();
    if (UHorizontalBoxSlot* S = BodyRow->AddChildToHorizontalBox(CenterOverlay))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    // 中央縮放/平移畫布（對應 Godot ScriptCanvas.cs:62-76）
    // 架構：CenterClip(clip) → CenterCanvas(無限畫布) → ZoomBox(UScaleBox UserSpecified) → CenterList
    // UScaleBox UserSpecified 在 layout 階段套用縮放，hit-testing 與視覺一致（非 RenderTransform）
    CenterClip = WidgetTree->ConstructWidget<UBorder>();
    CenterClip->SetBrush(MakeSolidBrush(FLinearColor(0.08f, 0.09f, 0.13f, 1.f)));
    CenterClip->SetClipping(EWidgetClipping::ClipToBoundsAlways);
    if (UOverlaySlot* S = CenterOverlay->AddChildToOverlay(CenterClip))
    {
        S->SetHorizontalAlignment(HAlign_Fill);
        S->SetVerticalAlignment(VAlign_Fill);
    }

    CenterCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    CenterClip->SetContent(CenterCanvas);

    ZoomBox = WidgetTree->ConstructWidget<UScaleBox>();
    ZoomBox->SetStretch(EStretch::UserSpecified);
    ZoomBox->SetUserSpecifiedScale(1.0f);

    CenterList = WidgetTree->ConstructWidget<UVerticalBox>();
    ZoomBox->SetContent(CenterList);

    ZoomBoxSlot = CenterCanvas->AddChildToCanvas(ZoomBox);
    ZoomBoxSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
    ZoomBoxSlot->SetPosition(FVector2D::ZeroVector);
    ZoomBoxSlot->SetAutoSize(true);
    ZoomBoxSlot->SetAlignment(FVector2D::ZeroVector);

    // 重置視角按鈕（左下角固定 overlay，對應 Godot ScriptCanvas.cs:137-151）
    {
        UButton* ResetBtn = WidgetTree->ConstructWidget<UButton>();
        ResetBtn->SetBackgroundColor(FLinearColor(0.08f, 0.08f, 0.12f, 0.85f));
        ResetBtn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnResetViewClicked);
        UTextBlock* ResetTxt = WidgetTree->ConstructWidget<UTextBlock>();
        ResetTxt->SetText(FText::FromString(TEXT("⌖ 重置視角")));
        ResetTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.70f)));
        {
            FSlateFontInfo F = ResetTxt->GetFont();
            F.Size = 10;
            ResetTxt->SetFont(F);
        }
        ResetBtn->AddChild(ResetTxt);
        if (UOverlaySlot* S = CenterOverlay->AddChildToOverlay(ResetBtn))
        {
            S->SetHorizontalAlignment(HAlign_Left);
            S->SetVerticalAlignment(VAlign_Bottom);
            S->SetPadding(FMargin(8.f, 0.f, 0.f, 8.f));
        }
    }

    TrashZone = CreateWidget<UBlockTrashZoneWidget>(this);
    TrashZone->Setup([this]() { RebuildList(); });
    if (UOverlaySlot* S = CenterOverlay->AddChildToOverlay(TrashZone))
    {
        S->SetHorizontalAlignment(HAlign_Right);
        S->SetVerticalAlignment(VAlign_Bottom);
        S->SetPadding(FMargin(0.f, 0.f, 8.f, 8.f));
    }

    // 右側統計面板容器（Godot AbilityEditorUI.cs:936-937，175px，內容於 Phase 5 填入）
    USizeBox* RightSize = WidgetTree->ConstructWidget<USizeBox>();
    RightSize->SetWidthOverride(175.f);
    RightPanel = WidgetTree->ConstructWidget<UBorder>();
    RightPanel->SetBrush(MakeSolidBrush(FLinearColor(0.14f, 0.14f, 0.18f, 1.f)));
    RightSize->SetContent(RightPanel);
    if (UHorizontalBoxSlot* S = BodyRow->AddChildToHorizontalBox(RightSize))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    BuildStatsPanel();

    return BodyRow;
}

void UBlockEditorWidget::OnBackClicked()
{
    // 對應 Godot AbilityEditorUI.cs:163-171：有巢狀層就 Pop，否則才是真正離開
    if (NavStack.Num() > 0)
    {
        NavStack.Pop();
        FSpellArray* Active = NavStack.Num() > 0 ? NavStack.Last().Key : RootSpell;
        CurrentSpell = Active;
        if (CurrentSpell && !CurrentSpell->Blocks.IsValid())
            CurrentSpell->SetBlocks({});
        CurrentBlocks = CurrentSpell ? CurrentSpell->Blocks.Get() : nullptr;
        RebuildList();
        RefreshHeaderState();
        return;
    }
    TryExitEditor();
}

void UBlockEditorWidget::OnGroupDot0Clicked() { SwitchGroup(0); }
void UBlockEditorWidget::OnGroupDot1Clicked() { SwitchGroup(1); }
void UBlockEditorWidget::OnGroupDot2Clicked() { SwitchGroup(2); }
void UBlockEditorWidget::OnGroupDot3Clicked() { SwitchGroup(3); }
void UBlockEditorWidget::OnGroupDot4Clicked() { SwitchGroup(4); }

// ══════════════════════════════════════════════════════════════════
//  Phase 2：左側 Palette（對應 Godot AbilityEditorUI.cs:419-836）
// ══════════════════════════════════════════════════════════════════

void UBlockEditorWidget::BuildPalette()
{
    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
    LeftPanel->SetContent(Root);

    // 三個主分頁（Godot AbilityEditorUI.cs:429-459）
    UHorizontalBox* MainTabRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(MainTabRow))
        S->SetHorizontalAlignment(HAlign_Fill);

    static const TCHAR* MainTabNames[3] = { TEXT("技能因子"), TEXT("積木"), TEXT("刻印") };
    // 2026-06-22 修崩潰：AddDynamic 是巨集會字串化第二個參數，不能傳函式指標變數
    // （見 BuildHeader() 上方同一個 bug 的詳細註解）。
    for (int32 i = 0; i < 3; ++i)
    {
        UButton* Btn = WidgetTree->ConstructWidget<UButton>();
        UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
        Txt->SetText(FText::FromString(MainTabNames[i]));
        Txt->SetJustification(ETextJustify::Center);
        Btn->AddChild(Txt);
        switch (i)
        {
            case 0: Btn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnMainTab0Clicked); break;
            case 1: Btn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnMainTab1Clicked); break;
            case 2: Btn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnMainTab2Clicked); break;
            default: break;
        }
        if (UHorizontalBoxSlot* S = MainTabRow->AddChildToHorizontalBox(Btn))
            S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        MainTabButtons[i] = Btn;
    }

    // Body：子分頁直排列（36px，Godot AbilityEditorUI.cs:481-497）+ 內容清單
    UHorizontalBox* PaletteBody = WidgetTree->ConstructWidget<UHorizontalBox>();
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(PaletteBody))
    {
        S->SetHorizontalAlignment(HAlign_Fill);
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    // Godot AbilityEditorUI.cs:464-497 bodyRow：scroll（內容清單）先 AddChild，subWrap（子標籤欄）
    // 後 AddChild——HBoxContainer 子節點順序＝視覺順序，先加的在左。之前 UE5 把 SubTabSize 先加進
    // PaletteBody，導致子標籤欄跑到左邊，內容清單被擠到右邊，跟 Godot 左右相反。交換加入順序修正。
    UScrollBox* ContentScroll = WidgetTree->ConstructWidget<UScrollBox>();
    PaletteContentList = WidgetTree->ConstructWidget<UVerticalBox>();
    ContentScroll->AddChild(PaletteContentList);
    if (UHorizontalBoxSlot* S = PaletteBody->AddChildToHorizontalBox(ContentScroll))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    USizeBox* SubTabSize = WidgetTree->ConstructWidget<USizeBox>();
    SubTabSize->SetWidthOverride(36.f);
    SubTabColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    SubTabSize->SetContent(SubTabColumn);
    if (UHorizontalBoxSlot* S = PaletteBody->AddChildToHorizontalBox(SubTabSize))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

    RebuildSubTabColumn();
    RefreshMainTabHighlight();
}

void UBlockEditorWidget::RefreshMainTabHighlight()
{
    for (int32 i = 0; i < 3; ++i)
    {
        if (!MainTabButtons[i]) continue;
        const bool bActive = (i == ActiveMainTab);
        MainTabButtons[i]->SetBackgroundColor(bActive
            ? FLinearColor(0.20f, 0.22f, 0.32f, 1.f)
            : FLinearColor(0.12f, 0.12f, 0.18f, 1.f));
    }
}

void UBlockEditorWidget::RebuildSubTabColumn()
{
    if (!SubTabColumn) return;
    SubTabColumn->ClearChildren();
    ActiveSubTab = 0;

    TArray<FString> Names;
    if (ActiveMainTab == 0)
        Names = { TEXT("範圍"), TEXT("武技"), TEXT("投射"), TEXT("被動"), TEXT("變幻"), TEXT("位移"), TEXT("召喚"), TEXT("領域") };
    else if (ActiveMainTab == 1)
        Names = { TEXT("控制"), TEXT("觸發"), TEXT("效果"), TEXT("變數"), TEXT("列表"), TEXT("實體"), TEXT("廣播"), TEXT("統計"), TEXT("向量"), TEXT("攔截"), TEXT("快照") };
    else
        Names = { TEXT("行動"), TEXT("白"), TEXT("橙"), TEXT("藍"), TEXT("紅"), TEXT("綠"), TEXT("紫"), TEXT("黃"), TEXT("黑"), TEXT("元素"), TEXT("法則") };

    // 2026-06-22 修崩潰：AddDynamic 是巨集會字串化第二個參數，不能傳函式指標變數
    // （見 BuildHeader() 註解）。
    for (int32 i = 0; i < Names.Num(); ++i)
    {
        UButton* Btn = WidgetTree->ConstructWidget<UButton>();
        Btn->SetBackgroundColor(FLinearColor(0.09f, 0.09f, 0.14f, 1.f));
        UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
        Txt->SetText(FText::FromString(Names[i]));
        Txt->SetAutoWrapText(true);
        Txt->SetJustification(ETextJustify::Center);
        Txt->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.62f, 0.72f)));
        Btn->AddChild(Txt);
        switch (i)
        {
            case 0:  Btn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnSubTab0Clicked);  break;
            case 1:  Btn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnSubTab1Clicked);  break;
            case 2:  Btn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnSubTab2Clicked);  break;
            case 3:  Btn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnSubTab3Clicked);  break;
            case 4:  Btn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnSubTab4Clicked);  break;
            case 5:  Btn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnSubTab5Clicked);  break;
            case 6:  Btn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnSubTab6Clicked);  break;
            case 7:  Btn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnSubTab7Clicked);  break;
            case 8:  Btn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnSubTab8Clicked);  break;
            case 9:  Btn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnSubTab9Clicked);  break;
            case 10: Btn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnSubTab10Clicked); break;
            default: break;
        }
        if (UVerticalBoxSlot* S = SubTabColumn->AddChildToVerticalBox(Btn))
            S->SetHorizontalAlignment(HAlign_Fill);
    }

    RebuildPaletteContent();
}

void UBlockEditorWidget::RebuildPaletteContent()
{
    if (!PaletteContentList) return;
    PaletteContentList->ClearChildren();

    if (ActiveMainTab == 0)
    {
        // 技能因子（Godot AbilityEditorUI.cs:536-668）
        static const ETotemType SubTypes[8] = {
            ETotemType::Area, ETotemType::Technique, ETotemType::Projectile, ETotemType::Passive,
            ETotemType::Morph, ETotemType::Displacement, ETotemType::Summon, ETotemType::Domain,
        };
        const ETotemType Filter = SubTypes[FMath::Clamp(ActiveSubTab, 0, 7)];

        for (const FTotemData& T : FTotemLibrary::AllTotems())
        {
            if (T.Type != Filter) continue;
            // Godot AbilityEditorUI.cs:620-668 BuildTotemContent：RequiredPlayerLevel 只用來
            // 在文字後面附加「LV{n}+」提示（line 629 lvTag），從未檢查/設定 btn.Disabled 或
            // 灰階顏色——技能因子（Totem）在這個遊戲階段永遠可點擊/可拖曳，不會被等級鎖住。
            // 等級鎖只存在於 BuildEngraveContent（line 814 locked = eng.RequiredPlayerLevel > PlayerLevel,
            // line 820 btn.Disabled = locked）。之前 UE5 把 Engraving 的鎖邏輯誤套用到 Totem 上，
            // 是純 UE5 發明的限制，Godot 原版完全沒有——移除，只保留提示文字。
            const FString LvTag = T.RequiredPlayerLevel > 1
                ? FString::Printf(TEXT("  LV%d+"), T.RequiredPlayerLevel) : TEXT("");
            const FText Label = FText::FromString(FString::Printf(TEXT("  %s%s  %dpt"),
                *T.DisplayName.ToString(), *LvTag, T.BaseAbilityPointCost));

            UPaletteItemWidget* Item = CreateWidget<UPaletteItemWidget>(this);
            const FName TotemId = T.TotemId;
            Item->Setup(Label, TotemTypeColor(T.Type), /*bInLocked=*/false,
                [TotemId](UBlockDragDropOp& Op) { Op.bIsTotemBlock = true; Op.PaletteTotemId = TotemId; });
            PaletteContentList->AddChild(Item);
        }
    }
    else if (ActiveMainTab == 2)
    {
        // 刻印（Godot AbilityEditorUI.cs:803-836）
        static const EEngraveColor SubColors[11] = {
            EEngraveColor::Action, EEngraveColor::White, EEngraveColor::Orange, EEngraveColor::Blue,
            EEngraveColor::Red, EEngraveColor::Green, EEngraveColor::Purple, EEngraveColor::Yellow,
            EEngraveColor::Black, EEngraveColor::Elemental, EEngraveColor::Law,
        };
        const EEngraveColor Filter = SubColors[FMath::Clamp(ActiveSubTab, 0, 10)];

        for (const FEngraveData& E : FTotemLibrary::AllEngravings())
        {
            if (E.Color != Filter) continue;
            const bool bLocked = E.RequiredPlayerLevel > PlayerLevel;
            const FString CostStr = E.bIsRestriction
                ? FString::Printf(TEXT("+%dpt"), E.BaseCost)
                : FString::Printf(TEXT("%dpt"), E.BaseCost);
            const FString LockTag = bLocked ? FString::Printf(TEXT("  🔒LV%d"), E.RequiredPlayerLevel) : TEXT("");
            const FText Label = FText::FromString(FString::Printf(TEXT("  %s  %s%s"),
                *E.DisplayName.ToString(), *CostStr, *LockTag));

            UPaletteItemWidget* Item = CreateWidget<UPaletteItemWidget>(this);
            const FName EngraveId = E.EngraveId;
            Item->Setup(Label, EngraveClrOf(E.Color), bLocked,
                [EngraveId](UBlockDragDropOp& Op) { Op.bIsEngravingBlock = true; Op.PaletteEngraveId = EngraveId; });
            PaletteContentList->AddChild(Item);
        }
    }
    else
    {
        // 積木（Godot AbilityEditorUI.cs:704-800 BuildBlockContent，16 類別 + 11 子分頁分組）
        struct FCat { const TCHAR* Label; TArray<EBlockType> Types; };
        static const TArray<FCat> Cats = {
            { TEXT("── 呼叫 ──"),     { EBlockType::InvokeTotem, EBlockType::InvokeSpell } },
            { TEXT("── 控制流 ──"),   { EBlockType::If, EBlockType::Evaluate, EBlockType::RepeatN,
                                        EBlockType::RepeatWhile, EBlockType::ForEachNearby, EBlockType::Wait,
                                        EBlockType::Sleep, EBlockType::Die, EBlockType::RandomChoice,
                                        EBlockType::SequentialGate } },
            { TEXT("── 觸發條件 ──"), { EBlockType::RisingEdge, EBlockType::FallingEdge,
                                        EBlockType::SinglePulse, EBlockType::AlternateTrigger } },
            { TEXT("── 偵測 ──"),     { EBlockType::DetectHpThreshold, EBlockType::DetectMpThreshold,
                                        EBlockType::DetectHitReceived, EBlockType::DetectEntityEnter,
                                        EBlockType::DetectProjectile, EBlockType::DetectAttack,
                                        EBlockType::DetectStatusChange } },
            { TEXT("── 發動模式 ──"), { EBlockType::SetActivationInstant, EBlockType::SetActivationDeclare,
                                        EBlockType::SetActivationSustained } },
            { TEXT("── 效果標示 ──"), { EBlockType::EffectLabel, EBlockType::OnEffectStart,
                                        EBlockType::OnEffectEnd } },
            { TEXT("── 執行時機 ──"), { EBlockType::EndOfChain, EBlockType::Discard } },
            { TEXT("── 變數 ──"),     { EBlockType::SetVar, EBlockType::GetVar, EBlockType::Compare,
                                        EBlockType::SetVarBool, EBlockType::GetVarBool } },
            { TEXT("── 列表 ──"),     { EBlockType::ListCreate, EBlockType::ListAppend, EBlockType::ListPop,
                                        EBlockType::ListDequeue, EBlockType::ListGet, EBlockType::ListSet,
                                        EBlockType::ListLength, EBlockType::ListContains,
                                        EBlockType::ListRemoveAt, EBlockType::ListClear } },
            { TEXT("── 實體 ──"),     { EBlockType::QueryNear, EBlockType::QueryNearest,
                                        EBlockType::GetEntityProp, EBlockType::SetEntityProp } },
            { TEXT("── 廣播 ──"),     { EBlockType::Broadcast, EBlockType::BroadcastAndWait,
                                        EBlockType::OnReceive } },
            { TEXT("── 計數器 ──"),   { EBlockType::TaskCounterSet, EBlockType::TaskCounterAdd,
                                        EBlockType::TaskCounterGet, EBlockType::TaskCounterOnReach,
                                        EBlockType::TaskCounterReset } },
            { TEXT("── 統計 ──"),     { EBlockType::GetBattleStat, EBlockType::GetComboCount,
                                        EBlockType::LoopcastIndex, EBlockType::SuccessCount } },
            { TEXT("── 向量 ──"),     { EBlockType::VecMake, EBlockType::VecGetComp, EBlockType::VecAdd,
                                        EBlockType::VecSub, EBlockType::VecScale, EBlockType::VecNegate,
                                        EBlockType::VecNorm, EBlockType::VecLength, EBlockType::VecDot,
                                        EBlockType::VecCross, EBlockType::VecFromEntity,
                                        EBlockType::FocalPoint, EBlockType::Raycast } },
            { TEXT("── 攔截 ──"),     { EBlockType::DamageShield, EBlockType::DeathGuard } },
            { TEXT("── 快照 ──"),     { EBlockType::Anchor, EBlockType::Rollback } },
        };
        static const TArray<TArray<int32>> Groups = {
            { 0, 1 }, { 2, 3 }, { 4, 5, 6 }, { 7 }, { 8 }, { 9 }, { 10 }, { 11, 12 }, { 13 }, { 14 }, { 15 },
        };
        const TArray<int32>& CatIdx = Groups[FMath::Clamp(ActiveSubTab, 0, 10)];

        for (int32 ci : CatIdx)
        {
            const FCat& Cat = Cats[ci];
            UTextBlock* CatLabel = WidgetTree->ConstructWidget<UTextBlock>();
            CatLabel->SetText(FText::FromString(Cat.Label));
            CatLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.50f, 0.50f, 0.62f)));
            PaletteContentList->AddChild(CatLabel);

            for (EBlockType BT : Cat.Types)
            {
                // Phase 4：FBlockUIRegistry 集中表精確中文名+色（取代之前的英文名+類別色）
                const FBlockUIDescriptor& Desc = FBlockUIRegistry::Get(BT);
                UPaletteItemWidget* Item = CreateWidget<UPaletteItemWidget>(this);
                Item->Setup(FText::FromString(TEXT("  ") + Desc.DisplayName.ToString()), Desc.Color, false,
                    [BT](UBlockDragDropOp& Op) { Op.PaletteBlockType = BT; });
                PaletteContentList->AddChild(Item);
            }
        }
    }
}

void UBlockEditorWidget::OnMainTab0Clicked() { ActiveMainTab = 0; RefreshMainTabHighlight(); RebuildSubTabColumn(); }
void UBlockEditorWidget::OnMainTab1Clicked() { ActiveMainTab = 1; RefreshMainTabHighlight(); RebuildSubTabColumn(); }
void UBlockEditorWidget::OnMainTab2Clicked() { ActiveMainTab = 2; RefreshMainTabHighlight(); RebuildSubTabColumn(); }

void UBlockEditorWidget::OnSubTab0Clicked()  { ActiveSubTab = 0;  RebuildPaletteContent(); }
void UBlockEditorWidget::OnSubTab1Clicked()  { ActiveSubTab = 1;  RebuildPaletteContent(); }
void UBlockEditorWidget::OnSubTab2Clicked()  { ActiveSubTab = 2;  RebuildPaletteContent(); }
void UBlockEditorWidget::OnSubTab3Clicked()  { ActiveSubTab = 3;  RebuildPaletteContent(); }
void UBlockEditorWidget::OnSubTab4Clicked()  { ActiveSubTab = 4;  RebuildPaletteContent(); }
void UBlockEditorWidget::OnSubTab5Clicked()  { ActiveSubTab = 5;  RebuildPaletteContent(); }
void UBlockEditorWidget::OnSubTab6Clicked()  { ActiveSubTab = 6;  RebuildPaletteContent(); }
void UBlockEditorWidget::OnSubTab7Clicked()  { ActiveSubTab = 7;  RebuildPaletteContent(); }
void UBlockEditorWidget::OnSubTab8Clicked()  { ActiveSubTab = 8;  RebuildPaletteContent(); }
void UBlockEditorWidget::OnSubTab9Clicked()  { ActiveSubTab = 9;  RebuildPaletteContent(); }
void UBlockEditorWidget::OnSubTab10Clicked() { ActiveSubTab = 10; RebuildPaletteContent(); }

// ══════════════════════════════════════════════════════════════════
//  Phase 3：中央積木卡片清單（對應 Godot ScratchCanvas::Rebuild/SyncFrom）
// ══════════════════════════════════════════════════════════════════

void UBlockEditorWidget::SetEditingSpell(FSpellArray* InSpell)
{
    RootSpell = InSpell;
    NavStack.Reset(); // 換一個技能整構時清空巢狀導覽（對應 Godot SwitchEditorGroup _navStack.Clear()）
    CurrentSpell = InSpell;
    if (CurrentSpell && !CurrentSpell->Blocks.IsValid())
        CurrentSpell->SetBlocks({}); // 全新技能整構：先給空陣列，Palette 拖拉才有東西可插入
    CurrentBlocks = CurrentSpell ? CurrentSpell->Blocks.Get() : nullptr;
    if (SpellNameBox)
        SpellNameBox->SetText(FText::FromString(CurrentSpell ? CurrentSpell->Name : FString()));
    // 這裡呼叫 Immediate 版本：SetEditingSpell 不是從某個子卡片/拖放區自己的事件回呼鏈裡呼叫的
    // （是 PlayerController 開啟編輯器時呼叫），同步重建沒有「正在銷毀呼叫者自身」的風險，
    // 而且下一行要立刻讀 bIsDirty 並重置回 false，必須在這裡就確定清單已經重建完成。
    RebuildListImmediate();
    bIsDirty = false; // 載入既有資料不算「使用者編輯」，RebuildListImmediate 內部會先設 true，這裡重置回乾淨狀態
    RefreshHeaderState();
}

void UBlockEditorWidget::RebuildList()
{
    // 2026-06-22 修復「拖曳調色盤卡片到畫布，卡片憑空消失」：這個函式常被
    // UBlockDropZoneWidget::NativeOnDrop / UBlockCardWidget::OnDeleteClicked 等
    // 仍在處理某個子 widget 自身輸入事件的呼叫鏈呼叫。若在這裡直接 ClearChildren()，
    // 會在 Slate 拖放事件分派堆疊還沒退出該 widget 時就把它整個摧毀——卡片因此「看起來」
    // 沒有落地，其實是重建時機過早造成的同步銷毀問題，不是 Insert 邏輯本身的 bug。
    // 對應 Godot 行為：Godot 的等價清單重建用 QueueFree()（延遲到當前 frame 結束才真正
    // 刪除節點），_DropData 回呼裡可以安全觸發整個重建。UE5 沒有原生等價物，改用
    // SetTimerForNextTick 延後到下一個 tick 才真正執行 ClearChildren()+重建，
    // 讓目前這一輪的 Slate 事件分派先完整結束。bRebuildPending 避免同一 tick 內重複排程。
    if (bRebuildPending) return;
    bRebuildPending = true;
    if (UWorld* World = GetWorld())
    {
        TWeakObjectPtr<UBlockEditorWidget> WeakThis(this);
        World->GetTimerManager().SetTimerForNextTick([WeakThis]()
        {
            if (UBlockEditorWidget* Self = WeakThis.Get())
            {
                Self->bRebuildPending = false;
                Self->RebuildListImmediate();
            }
        });
    }
    else
    {
        // 編輯器/無 World 環境保險：找不到 World 就退回同步執行，至少行為正確
        bRebuildPending = false;
        RebuildListImmediate();
    }
}

void UBlockEditorWidget::RebuildListImmediate()
{
    // Godot AbilityEditorUI.cs:865: bool inserted = AutoInsertBaseEngravings(); if (inserted) SyncCanvas();
    // UE5：在重建前先插入，重建結果即包含插入後的積木序列，不需二次 RebuildList()
    AutoInsertBaseEngravings();

    bIsDirty = true; // 對應 Godot 幾乎每個編輯 callback 都設 _isDirty=true；SetEditingSpell 載入後會重置回 false
    if (!CenterList) return;
    CenterList->ClearChildren();
    if (!CurrentBlocks)
    {
        RefreshStatsPanel();
        return;
    }

    UBlockCardWidget::BuildBlockList(this, CenterList, *CurrentBlocks, 0,
        [this]() { RebuildList(); },
        [this](FBlockNode* B) { EnterContainerEffect(B); });

    // 對應 Godot 每次 canvas 變動後呼叫 SyncSlotsFromBlocks（AbilityEditorUI.cs:866 附近），
    // 讓 Slots/GlobalEngravings/Container 跟積木樹保持同步，AP/MP 數字才能即時反映變動
    if (CurrentSpell)
        FSpellSlotSync::SyncSlotsFromBlocks(*CurrentSpell);

    RefreshStatsPanel();
}

// ══════════════════════════════════════════════════════════════════
//  Phase 5：右側統計面板（對應 Godot AbilityEditorUI.cs:934-1122）
// ══════════════════════════════════════════════════════════════════

void UBlockEditorWidget::BuildStatsPanel()
{
    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
    RightPanel->SetContent(Root);

    auto AddSectionLabel = [&](const FString& Text)
    {
        UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>();
        Lbl->SetText(FText::FromString(Text));
        Lbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.75f)));
        if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Lbl))
            S->SetPadding(FMargin(6.f, 8.f, 6.f, 2.f));
    };

    // 能力點統計（Godot AbilityEditorUI.cs:952-979）
    AddSectionLabel(TEXT("能力點統計"));
    ApValueLabel = WidgetTree->ConstructWidget<UTextBlock>();
    ApValueLabel->SetText(FText::FromString(TEXT("0 點")));
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(ApValueLabel))
        S->SetPadding(FMargin(6.f, 0.f));

    ApBar = WidgetTree->ConstructWidget<UProgressBar>();
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(ApBar))
        S->SetPadding(FMargin(6.f, 2.f));

    // 基礎 MP 消耗（Godot AbilityEditorUI.cs:986-999）
    AddSectionLabel(TEXT("基礎消耗"));
    BaseMpSpin = WidgetTree->ConstructWidget<USpinBox>();
    BaseMpSpin->SetMinValue(0.f);
    BaseMpSpin->SetMaxValue(9999.f);
    BaseMpSpin->SetDelta(1.f);
    BaseMpSpin->OnValueChanged.AddDynamic(this, &UBlockEditorWidget::OnBaseMpChanged);
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(BaseMpSpin))
        S->SetPadding(FMargin(6.f, 0.f));

    // MP 消耗分解（Godot AbilityEditorUI.cs:1005-1116）
    AddSectionLabel(TEXT("MP 消耗"));
    MpBreakdownList = WidgetTree->ConstructWidget<UVerticalBox>();
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(MpBreakdownList))
        S->SetPadding(FMargin(6.f, 0.f));

    // 技能整構摘要（Godot AbilityEditorUI.cs:1118-1122）
    AddSectionLabel(TEXT("技能整構摘要"));
    UScrollBox* DescScroll = WidgetTree->ConstructWidget<UScrollBox>();
    DescriptionLabel = WidgetTree->ConstructWidget<UTextBlock>();
    DescriptionLabel->SetAutoWrapText(true);
    DescScroll->AddChild(DescriptionLabel);
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(DescScroll))
    {
        S->SetPadding(FMargin(6.f, 0.f));
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    // 儲存按鈕（Phase 7，對應 Godot SaveSpell() 觸發點）
    SaveButton = WidgetTree->ConstructWidget<UButton>();
    SaveButton->SetBackgroundColor(FLinearColor(0.20f, 0.45f, 0.25f, 1.f));
    {
        UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
        Txt->SetText(FText::FromString(TEXT("儲存技能整構")));
        Txt->SetJustification(ETextJustify::Center);
        SaveButton->AddChild(Txt);
    }
    SaveButton->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnSaveButtonClicked);
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(SaveButton))
        S->SetPadding(FMargin(6.f, 8.f));

    RefreshStatsPanel();
}

void UBlockEditorWidget::RefreshStatsPanel()
{
    if (!ApValueLabel) return; // 面板尚未建好（BuildStatsPanel 第一次呼叫時 RefreshStatsPanel 會在最後才跑）

    if (!CurrentSpell)
    {
        ApValueLabel->SetText(FText::FromString(TEXT("0 點")));
        if (ApBar) ApBar->SetPercent(0.f);
        if (MpBreakdownList) MpBreakdownList->ClearChildren();
        if (DescriptionLabel) DescriptionLabel->SetText(FText::GetEmpty());
        return;
    }

    // AP（對應 Godot AbilityEditorUI.cs:1067-1084）
    const int32 Ap  = FAbilityPointCalculator::CalculateTotalCost(*CurrentSpell);
    const int32 Cap = FAbilityPointCalculator::TierApCap(PlayerLevel);
    const bool  bOver = FAbilityPointCalculator::ExceedsLevelCap(*CurrentSpell, PlayerLevel);
    ApValueLabel->SetText(FText::FromString(FString::Printf(TEXT("%d 點"), Ap)));
    ApValueLabel->SetColorAndOpacity(FSlateColor(bOver
        ? FLinearColor(1.f, 0.3f, 0.3f) : FLinearColor(0.9f, 0.9f, 0.9f)));
    if (ApBar) ApBar->SetPercent(Cap > 0 ? FMath::Clamp((float)Ap / (float)Cap, 0.f, 1.f) : 0.f);

    // 基礎 MP 消耗（避免 SetValue 觸發 OnValueChanged 造成無窮迴圈，先檢查再設）
    if (BaseMpSpin && !FMath::IsNearlyEqual(BaseMpSpin->GetValue(), CurrentSpell->BaseMpCost))
        BaseMpSpin->SetValue(CurrentSpell->BaseMpCost);

    // MP 按類型分解（對應 Godot AbilityEditorUI.cs:1086-1116）
    if (MpBreakdownList)
    {
        MpBreakdownList->ClearChildren();
        const TMap<FName, float> ByType = FAbilityPointCalculator::CalculateSlotCostByType(*CurrentSpell);
        if (ByType.Num() == 0)
        {
            UTextBlock* Fallback = WidgetTree->ConstructWidget<UTextBlock>();
            Fallback->SetText(FText::FromString(FString::Printf(TEXT("%.0f 點"),
                FAbilityPointCalculator::CalculateMpCost(*CurrentSpell))));
            MpBreakdownList->AddChildToVerticalBox(Fallback);
        }
        else
        {
            for (const auto& Pair : ByType)
            {
                const FManaType* MT = FManaTypeRegistry::Get().Find(Pair.Key);
                const FString TypeName = MT ? MT->DisplayName.ToString() : Pair.Key.ToString();
                UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>();
                Row->SetText(FText::FromString(FString::Printf(TEXT("%s：%.0f 點"), *TypeName, Pair.Value)));
                Row->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.85f, 1.f)));
                MpBreakdownList->AddChildToVerticalBox(Row);
            }
        }
        if (CurrentSpell->HasUnboundMpBlocks())
        {
            UTextBlock* Warn = WidgetTree->ConstructWidget<UTextBlock>();
            Warn->SetText(FText::FromString(TEXT("⚠ 部分插槽未指定 MP 種類")));
            Warn->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.55f, 0.3f)));
            MpBreakdownList->AddChildToVerticalBox(Warn);
        }
    }

    // 技能摘要（對應 Godot AbilityEditorUI.cs:1118-1122）
    if (DescriptionLabel)
        DescriptionLabel->SetText(FText::FromString(FSpellDescriptionGenerator::GenerateStructured(*CurrentSpell)));
}

void UBlockEditorWidget::OnBaseMpChanged(float NewValue)
{
    if (!CurrentSpell) return;
    CurrentSpell->BaseMpCost = NewValue;
    RefreshStatsPanel();
}

// ══════════════════════════════════════════════════════════════════
//  Phase 6：容器巢狀導覽（對應 Godot AbilityEditorUI.cs:29-34/363-404）
// ══════════════════════════════════════════════════════════════════

FString UBlockEditorWidget::BuildBreadcrumb() const
{
    // 對應 Godot BuildBreadcrumb（AbilityEditorUI.cs:394-404）
    TArray<FString> Parts;
    Parts.Add(RootSpell && !RootSpell->Name.IsEmpty() ? RootSpell->Name : TEXT("（未命名）"));
    for (const TPair<FSpellArray*, FString>& Entry : NavStack)
        Parts.Add(Entry.Value);
    return FString::Join(Parts, TEXT(" › "));
}

void UBlockEditorWidget::RefreshHeaderState()
{
    // 對應 Godot RefreshHeaderState（AbilityEditorUI.cs:385-391）
    const bool bInContainer = NavStack.Num() > 0;
    if (BackButton)
    {
        // UButton 沒有直接 SetText；改設內部 TextBlock —— 這裡簡化只用 Tooltip/不變更文字，
        // 真正可變文字交給 BreadcrumbLabel 顯示巢狀狀態即可（按鈕本身意圖不變：返回/上一層）
    }
    if (BreadcrumbLabel)
    {
        BreadcrumbLabel->SetVisibility(bInContainer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (bInContainer)
            BreadcrumbLabel->SetText(FText::FromString(BuildBreadcrumb()));
    }
}

void UBlockEditorWidget::EnterContainerEffect(FBlockNode* EngravingBlock)
{
    // 對應 Godot BlockDoubleClicked + EnterContainerEffect（AbilityEditorUI.cs:363-383,918-928）
    if (!EngravingBlock || EngravingBlock->Type != EBlockType::Engraving) return;
    if (NavStack.Num() >= FSafetyGuard::MaxContainerDepth) return;

    const FInstancedStruct* IS = EngravingBlock->Params.Find(TEXT("args"));
    const FEngravingBlockArgs* Args = IS ? IS->GetPtr<FEngravingBlockArgs>() : nullptr;
    if (!Args || Args->EngraveId.IsNone()) return;
    if (!FTotemLibrary::ContainerActionIds().Contains(Args->EngraveId)) return;

    const FEngraveData* Data = FTotemLibrary::FindEngraving(Args->EngraveId);
    const FString DisplayName = Data ? Data->DisplayName.ToString() : Args->EngraveId.ToString();

    ShowConfirmDialog(
        TEXT("進入容器效果"),
        FString::Printf(TEXT("編輯「%s」的內部效果？"), *DisplayName),
        TEXT("進入"), TEXT("取消"),
        [this, DisplayName]()
        {
            if (!CurrentSpell) return;
            if (!CurrentSpell->ContainerEffect.IsValid())
                CurrentSpell->ContainerEffect = MakeShared<FSpellArray>();
            FSpellArray* Next = CurrentSpell->ContainerEffect.Get();
            NavStack.Add({ Next, DisplayName });
            CurrentSpell = Next;
            if (!CurrentSpell->Blocks.IsValid())
                CurrentSpell->SetBlocks({});
            CurrentBlocks = CurrentSpell->Blocks.Get();
            RebuildList();
            RefreshHeaderState();
        });
}

void UBlockEditorWidget::ShowConfirmDialog(const FString& Title, const FString& Message,
                                            const FString& ConfirmLabel, const FString& CancelLabel,
                                            TFunction<void()> OnConfirm, TFunction<void()> OnCancel)
{
    if (!ConfirmOverlay) return;
    ConfirmOverlay->ClearChildren();

    UBorder* DialogBox = WidgetTree->ConstructWidget<UBorder>();
    DialogBox->SetBrush(MakeSolidBrush(FLinearColor(0.14f, 0.14f, 0.18f, 1.f)));
    DialogBox->SetPadding(FMargin(16.f));

    UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>();
    DialogBox->SetContent(VBox);

    UTextBlock* TitleLbl = WidgetTree->ConstructWidget<UTextBlock>();
    TitleLbl->SetText(FText::FromString(Title));
    TitleLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.88f, 0.95f)));
    if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(TitleLbl))
        S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

    UTextBlock* MsgLbl = WidgetTree->ConstructWidget<UTextBlock>();
    MsgLbl->SetText(FText::FromString(Message));
    MsgLbl->SetAutoWrapText(true);
    if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(MsgLbl))
        S->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    VBox->AddChildToVerticalBox(BtnRow);

    UButton* ConfirmBtn = WidgetTree->ConstructWidget<UButton>();
    ConfirmBtn->SetBackgroundColor(FLinearColor(0.20f, 0.45f, 0.25f, 1.f));
    {
        UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
        Txt->SetText(FText::FromString(ConfirmLabel));
        ConfirmBtn->AddChild(Txt);
    }
    PendingDialogConfirm = MoveTemp(OnConfirm);
    ConfirmBtn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnConfirmDialogConfirmClicked);
    if (UHorizontalBoxSlot* S = BtnRow->AddChildToHorizontalBox(ConfirmBtn))
        S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));

    // CancelLabel 空字串 = 純確認型對話框（對應 Godot AcceptDialog，例如儲存失敗的錯誤訊息），
    // 不需要第二顆按鈕
    if (!CancelLabel.IsEmpty())
    {
        UButton* CancelBtn = WidgetTree->ConstructWidget<UButton>();
        CancelBtn->SetBackgroundColor(FLinearColor(0.30f, 0.20f, 0.20f, 1.f));
        {
            UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
            Txt->SetText(FText::FromString(CancelLabel));
            CancelBtn->AddChild(Txt);
        }
        PendingDialogCancel = MoveTemp(OnCancel);
        CancelBtn->OnClicked.AddDynamic(this, &UBlockEditorWidget::OnConfirmDialogCancelClicked);
        BtnRow->AddChildToHorizontalBox(CancelBtn);
    }
    ConfirmOverlay->SetVisibility(ESlateVisibility::Visible);

    ConfirmOverlay->SetContent(DialogBox);
    ConfirmOverlay->SetHorizontalAlignment(HAlign_Center);
    ConfirmOverlay->SetVerticalAlignment(VAlign_Center);
}

void UBlockEditorWidget::OnConfirmDialogConfirmClicked()
{
    if (ConfirmOverlay) ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
    TFunction<void()> Callback = MoveTemp(PendingDialogConfirm);
    PendingDialogConfirm = nullptr;
    PendingDialogCancel  = nullptr;
    if (Callback) Callback();
}

void UBlockEditorWidget::OnConfirmDialogCancelClicked()
{
    if (ConfirmOverlay) ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
    TFunction<void()> Callback = MoveTemp(PendingDialogCancel);
    PendingDialogConfirm = nullptr;
    PendingDialogCancel  = nullptr;
    if (Callback) Callback();
}

// ══════════════════════════════════════════════════════════════════
//  Phase 7：儲存/驗證/未儲存確認/技能組切換（對應 Godot AbilityEditorUI.cs:
//  231-327,1276-1345）
// ══════════════════════════════════════════════════════════════════

void UBlockEditorWidget::OnSpellNameCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    if (!CurrentSpell) return;
    CurrentSpell->Name = Text.ToString();
    bIsDirty = true;
    RefreshStatsPanel(); // 名稱也出現在技能摘要文字裡，對應 Godot RefreshDescription()
}

void UBlockEditorWidget::OnSaveButtonClicked()
{
    HandleSaveClicked();
}

void UBlockEditorWidget::HandleSaveClicked()
{
    // 對應 Godot SaveSpell()（AbilityEditorUI.cs:1276-1345）：先同步 Slots，5 項驗證
    // 全過才真正廣播給 PlayerController 寫回 Loadout
    if (!RootSpell) return;
    TArray<TUniquePtr<FBlockNode>> EmptyBlocks;
    TArray<TUniquePtr<FBlockNode>>* RootBlocks = RootSpell->Blocks.IsValid() ? RootSpell->Blocks.Get() : &EmptyBlocks;

    FSpellSlotSync::SyncSlotsFromBlocks(*RootSpell);
    const TArray<FString> Errors = FSpellSlotSync::ValidateSpell(*RootSpell, *RootBlocks, PlayerLevel);
    if (Errors.Num() > 0)
    {
        ShowValidationErrors(Errors);
        return;
    }

    bIsDirty = false;
    if (StatusLabel)
        StatusLabel->SetText(FText::FromString(
            FString::Printf(TEXT("✓ 槽位 %d「%s」已存"), ActiveSlotIndex + 1, *RootSpell->Name)));
    OnSaveSpell.ExecuteIfBound(RootSpell->Name, ActiveSlotIndex);
}

void UBlockEditorWidget::ShowValidationErrors(const TArray<FString>& Errors)
{
    FString Msg;
    for (const FString& E : Errors)
        Msg += TEXT("• ") + E + TEXT("\n");
    ShowConfirmDialog(TEXT("⚠ 儲存失敗"), Msg.TrimEnd(), TEXT("確認"), FString(), nullptr);
}

void UBlockEditorWidget::TryExitEditor()
{
    // 對應 Godot TryExitEditor（AbilityEditorUI.cs:264-327）
    if (!bIsDirty)
    {
        OnCloseRequested.ExecuteIfBound();
        return;
    }
    ShowConfirmDialog(
        TEXT("未儲存的變更"), TEXT("技能整構尚未儲存，是否儲存後離開？"),
        TEXT("儲存並離開"), TEXT("捨棄變更"),
        [this]()
        {
            HandleSaveClicked();
            // 驗證失敗時 HandleSaveClicked 已經彈出錯誤對話框且 bIsDirty 仍是 true，不離開
            if (!bIsDirty) OnCloseRequested.ExecuteIfBound();
        },
        [this]()
        {
            bIsDirty = false;
            OnCloseRequested.ExecuteIfBound();
        });
}

void UBlockEditorWidget::SwitchGroup(int32 NewGroupIndex)
{
    // 對應 Godot SwitchEditorGroup（AbilityEditorUI.cs:231-250）。UE5 直接用指標編輯原始
    // 資料（CurrentSpell 本來就指向 Loadout 裡的 FSpellArray），不像 Godot 需要本地 _spells[]
    // 緩衝寫回——這一步在 UE5 架構下是多餘的，切組時資料早已經是最新的。
    if (!SpellGroups || NewGroupIndex == SpellGroups->ActiveGroupIndex) return;
    SpellGroups->SetActiveGroup(NewGroupIndex);

    FSpellLoadout& Loadout = SpellGroups->GetActiveLoadout();
    const int32 SlotIdx = FMath::Clamp(ActiveSlotIndex, 0, FSpellLoadout::MaxSlots - 1);
    SetEditingSpell(&Loadout.Slots[SlotIdx]);
    RefreshGroupDotHighlight();
}

void UBlockEditorWidget::RefreshGroupDotHighlight()
{
    // 對應 Godot RefreshGroupDots（AbilityEditorUI.cs:252-262）
    const int32 Active = SpellGroups ? SpellGroups->ActiveGroupIndex : 0;
    for (int32 i = 0; i < GroupDotCount; ++i)
    {
        if (!GroupDots[i]) continue;
        GroupDots[i]->SetBackgroundColor(i == Active
            ? FLinearColor(0.40f, 0.85f, 1.00f) : FLinearColor(0.50f, 0.50f, 0.60f));
    }
}

// ══════════════════════════════════════════════════════════════════
// 編輯器設定（Godot AbilityEditorUI.cs:211-358 + 1204-1227）
// ══════════════════════════════════════════════════════════════════

void UBlockEditorWidget::LoadEditorSettings()
{
    // Godot EditorSettings.AutoInsertBaseEngraving（持久化，預設 true）
    bool bVal = true;
    GConfig->GetBool(BlockEditorCfgSection, TEXT("AutoInsertBaseEngraving"), bVal, BlockEditorCfgPath());
    bAutoInsertBaseEngraving = bVal;
}

void UBlockEditorWidget::SaveEditorSettings()
{
    GConfig->SetBool(BlockEditorCfgSection, TEXT("AutoInsertBaseEngraving"), bAutoInsertBaseEngraving, BlockEditorCfgPath());
    GConfig->Flush(false, BlockEditorCfgPath());
}

void UBlockEditorWidget::ShowEditorSettingsPopup()
{
    // Godot AbilityEditorUI.cs:331-358 ShowSettingsPopup
    if (!EditorSettingsOverlay) return;
    EditorSettingsOverlay->ClearChildren();

    // 彈窗主體：330px 寬，深色背景，內邊距 14/10/14/12（對應 Godot MarginContainer override）
    UBorder* PopupBox = WidgetTree->ConstructWidget<UBorder>();
    PopupBox->SetBrush(MakeSolidBrush(FLinearColor(0.14f, 0.14f, 0.18f, 1.f)));
    PopupBox->SetPadding(FMargin(14.f, 10.f, 14.f, 12.f));

    USizeBox* PopupSize = WidgetTree->ConstructWidget<USizeBox>();
    PopupSize->SetWidthOverride(330.f);
    PopupSize->SetContent(PopupBox);

    UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>();
    PopupBox->SetContent(VBox);

    // 標題（Godot：Label "編輯器設定"，font_size 13，色 0.85,0.85,0.45）
    UTextBlock* TitleTxt = WidgetTree->ConstructWidget<UTextBlock>();
    TitleTxt->SetText(FText::FromString(TEXT("編輯器設定")));
    TitleTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.45f)));
    if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(TitleTxt))
        S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

    // 分隔線（Godot：HSeparator）
    USizeBox* SepSize = WidgetTree->ConstructWidget<USizeBox>();
    SepSize->SetHeightOverride(1.f);
    UBorder* SepLine = WidgetTree->ConstructWidget<UBorder>();
    SepLine->SetBrush(MakeSolidBrush(FLinearColor(0.5f, 0.5f, 0.5f, 1.f)));
    SepSize->SetContent(SepLine);
    if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(SepSize))
        S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

    // 勾選框（Godot AbilityEditorUI.cs:351-354：CheckButton + Toggled → AutoInsertBaseEngraving）
    UHorizontalBox* CheckRow = WidgetTree->ConstructWidget<UHorizontalBox>();

    UCheckBox* Check = WidgetTree->ConstructWidget<UCheckBox>();
    Check->SetIsChecked(bAutoInsertBaseEngraving);
    Check->OnCheckStateChanged.AddLambda([this](bool bChecked)
    {
        bAutoInsertBaseEngraving = bChecked;
        SaveEditorSettings();
    });
    if (UHorizontalBoxSlot* CS = CheckRow->AddChildToHorizontalBox(Check))
    {
        CS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        CS->SetVerticalAlignment(VAlign_Center);
    }

    UTextBlock* CheckTxt = WidgetTree->ConstructWidget<UTextBlock>();
    CheckTxt->SetText(FText::FromString(TEXT("技能因子加入時自動插入基礎 Action 刻印")));
    if (UHorizontalBoxSlot* CS = CheckRow->AddChildToHorizontalBox(CheckTxt))
    {
        CS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        CS->SetPadding(FMargin(6.f, 0.f));
        CS->SetVerticalAlignment(VAlign_Center);
    }

    if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(CheckRow))
        S->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));

    // 關閉按鈕（Godot 無此按鈕，用點擊彈窗外關閉；UE5 加關閉鈕較直觀）
    UButton* CloseBtn = WidgetTree->ConstructWidget<UButton>();
    CloseBtn->SetBackgroundColor(FLinearColor(0.25f, 0.25f, 0.30f, 1.f));
    UTextBlock* CloseTxt = WidgetTree->ConstructWidget<UTextBlock>();
    CloseTxt->SetText(FText::FromString(TEXT("關閉")));
    CloseBtn->AddChild(CloseTxt);
    CloseBtn->OnClicked.AddLambda([this]()
    {
        if (EditorSettingsOverlay)
            EditorSettingsOverlay->SetVisibility(ESlateVisibility::Collapsed);
    });
    if (UVerticalBoxSlot* S = VBox->AddChildToVerticalBox(CloseBtn))
        S->SetHorizontalAlignment(HAlign_Center);

    EditorSettingsOverlay->SetContent(PopupSize);
    EditorSettingsOverlay->SetHorizontalAlignment(HAlign_Center);
    EditorSettingsOverlay->SetVerticalAlignment(VAlign_Center);
    EditorSettingsOverlay->SetVisibility(ESlateVisibility::Visible);
}

bool UBlockEditorWidget::AutoInsertBaseEngravings()
{
    // Godot AbilityEditorUI.cs:1204-1227
    if (!bAutoInsertBaseEngraving || !CurrentBlocks) return false;

    const TMap<FName, FName>& IdMap = FTotemLibrary::DefaultActionEngraveId();
    bool bInserted = false;

    for (int32 i = 0; i < CurrentBlocks->Num(); ++i)
    {
        FBlockNode* Node = (*CurrentBlocks)[i].Get();
        if (!Node || Node->Type != EBlockType::Totem) continue;

        FInstancedStruct* ParamsPtr = Node->Params.Find(TEXT("args"));
        if (!ParamsPtr) continue;

        FTotemBlockArgs* Args = ParamsPtr->GetMutablePtr<FTotemBlockArgs>();
        if (!Args || Args->bActInserted) continue;

        Args->bActInserted = true;

        const FName* ActId = IdMap.Find(Args->TotemId);
        if (!ActId) continue; // "custom" 或未知 → 標記但不插入（對應 Godot AbilityEditorUI.cs:677）

        TUniquePtr<FBlockNode> EngNode = MakeUnique<FBlockNode>();
        EngNode->Type = EBlockType::Engraving;
        FEngravingBlockArgs EngArgs;
        EngArgs.EngraveId = *ActId;
        EngArgs.Points    = 0.f;
        EngNode->Params.Add(TEXT("args"), FInstancedStruct::Make<FEngravingBlockArgs>(EngArgs));

        CurrentBlocks->Insert(MoveTemp(EngNode), i + 1);
        ++i; // 跳過剛插入的刻印（對應 Godot AbilityEditorUI.cs:1223 i++）
        bInserted = true;
    }
    return bInserted;
}

// ══════════════════════════════════════════════════════════════════
//  畫布縮放/平移（對應 Godot ScriptCanvas.cs 互動邏輯）
// ══════════════════════════════════════════════════════════════════

FReply UBlockEditorWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Ctrl+左鍵拖曳平移（Godot ScriptCanvas.cs:287-295）
    // NativeOnPreviewMouseButtonDown 是 top-down 路由：比子 widget 先收到，可安全截取 Ctrl+Left
    // 返回 Handled + CaptureMouse 後子 widget 不再收到此 LeftButton 事件（畫布平移優先）
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
        && InMouseEvent.IsControlDown()
        && CenterClip)
    {
        const FGeometry& ClipGeo = CenterClip->GetCachedGeometry();
        const FVector2D  LocalPos = ClipGeo.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        const FVector2D  ClipSize = ClipGeo.GetLocalSize();
        const bool bOverCanvas = LocalPos.X >= 0.f && LocalPos.Y >= 0.f
                              && LocalPos.X <= ClipSize.X && LocalPos.Y <= ClipSize.Y;
        if (bOverCanvas)
        {
            bPanning         = true;
            bPanWithLeft     = true;
            PanDragStart     = InMouseEvent.GetScreenSpacePosition();
            PanOriginAtStart = PanOffset;
            TSharedPtr<SWidget> Slate = GetCachedWidget();
            if (Slate.IsValid())
                return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
            return FReply::Handled();
        }
    }
    return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UBlockEditorWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // 只在滑鼠位於中央畫布內時縮放（Godot ScriptCanvas.cs:271-276 GetGlobalRect().HasPoint 判斷）
    if (!CenterClip) return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);

    const FGeometry& ClipGeo  = CenterClip->GetCachedGeometry();
    const FVector2D  ScreenPos = InMouseEvent.GetScreenSpacePosition();
    const FVector2D  LocalPos  = ClipGeo.AbsoluteToLocal(ScreenPos);
    const FVector2D  ClipSize  = ClipGeo.GetLocalSize();
    const bool bOverCanvas = LocalPos.X >= 0.f && LocalPos.Y >= 0.f
                          && LocalPos.X <= ClipSize.X && LocalPos.Y <= ClipSize.Y;
    if (!bOverCanvas)
        return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);

    // ZoomStep=0.12 對應 Godot ScriptCanvas.cs:58
    const float Delta = InMouseEvent.GetWheelDelta() > 0.f ? 0.12f : -0.12f;
    ZoomAt(LocalPos, Delta);
    return FReply::Handled();
}

FReply UBlockEditorWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // 中鍵按下 → 開始平移（Godot ScriptCanvas.cs:278-286 MouseButton.Middle）
    if (InMouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton && CenterClip)
    {
        const FGeometry& ClipGeo  = CenterClip->GetCachedGeometry();
        const FVector2D  LocalPos  = ClipGeo.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        const FVector2D  ClipSize  = ClipGeo.GetLocalSize();
        const bool bOverCanvas = LocalPos.X >= 0.f && LocalPos.Y >= 0.f
                              && LocalPos.X <= ClipSize.X && LocalPos.Y <= ClipSize.Y;
        if (bOverCanvas)
        {
            bPanning         = true;
            PanDragStart     = InMouseEvent.GetScreenSpacePosition();
            PanOriginAtStart = PanOffset;
            TSharedPtr<SWidget> Slate = GetCachedWidget();
            if (Slate.IsValid())
                return FReply::Handled().CaptureMouse(Slate.ToSharedRef());
            return FReply::Handled();
        }
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UBlockEditorWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // 平移中更新 PanOffset（Godot ScriptCanvas.cs:236-247）
    if (bPanning)
    {
        // Ctrl+Left 模式：Ctrl 放開即結束平移（Godot ScriptCanvas.cs:239-243）
        if (bPanWithLeft && !InMouseEvent.IsControlDown())
        {
            bPanning = bPanWithLeft = false;
            return FReply::Handled().ReleaseMouseCapture();
        }
        PanOffset = PanOriginAtStart + (InMouseEvent.GetScreenSpacePosition() - PanDragStart);
        ApplyCanvasTransform();
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UBlockEditorWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // 中鍵放開 → 結束平移（Godot ScriptCanvas.cs:300-305）
    if (InMouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton && bPanning && !bPanWithLeft)
    {
        bPanning = false;
        return FReply::Handled().ReleaseMouseCapture();
    }
    // Ctrl+左鍵放開 → 結束平移（Godot ScriptCanvas.cs:307-315）
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bPanning && bPanWithLeft)
    {
        bPanning = bPanWithLeft = false;
        return FReply::Handled().ReleaseMouseCapture();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UBlockEditorWidget::ZoomAt(FVector2D LocalMousePos, float Delta)
{
    // 以滑鼠為軸心縮放（Godot ScriptCanvas.cs:460-468 ZoomAt）
    constexpr float ZoomMin = 0.20f, ZoomMax = 3.00f;
    const float NewZoom = FMath::Clamp(ZoomFactor + Delta, ZoomMin, ZoomMax);
    if (FMath::IsNearlyEqual(NewZoom, ZoomFactor)) return;

    // pivot = 滑鼠在 CenterClip 的本機座標
    // pan  = pivot - (pivot - pan) * (newZoom / oldZoom)
    const FVector2D Pivot = LocalMousePos;
    PanOffset  = Pivot - (Pivot - PanOffset) * (NewZoom / ZoomFactor);
    ZoomFactor = NewZoom;
    ApplyCanvasTransform();
}

void UBlockEditorWidget::ApplyCanvasTransform()
{
    // 套用 pan/zoom 到 ZoomBox（Godot ScriptCanvas.cs:454-457）
    // UScaleBox UserSpecified 在 layout 階段縮放 → hit-testing 正確（非 RenderTransform）
    if (!ZoomBox || !ZoomBoxSlot) return;
    ZoomBox->SetUserSpecifiedScale(ZoomFactor);
    ZoomBoxSlot->SetPosition(PanOffset);
}

void UBlockEditorWidget::ResetView()
{
    // 重置視角到原點 1:1（Godot ScriptCanvas.cs:177-181）
    ZoomFactor = 1.0f;
    PanOffset  = FVector2D::ZeroVector;
    ApplyCanvasTransform();
}

void UBlockEditorWidget::OnResetViewClicked()
{
    ResetView();
}
