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
#include "UPaletteItemWidget.h"
#include "UBlockDragDropOp.h"
#include "UBlockCardWidget.h"
#include "TotemLibrary.h"
#include "FBlockNode.h"

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

// 積木分頁的「每類別一色」近似色表（對應 Godot ScratchCanvas.cs 逐型別色表的簡化版；
// Phase 4 建 FBlockUIDescriptor 集中表時會補上逐積木型別精確色，這裡先用類別色頂著）
static FLinearColor BlockCategoryColor(int32 CatIndex)
{
    static const FLinearColor Colors[16] = {
        FLinearColor(1.00f, 0.72f, 0.35f), // 0 呼叫（橙）
        FLinearColor(0.65f, 0.95f, 0.30f), // 1 控制流（黃綠）
        FLinearColor(0.38f, 0.88f, 0.88f), // 2 觸發條件（青）
        FLinearColor(1.00f, 0.42f, 0.42f), // 3 偵測（紅）
        FLinearColor(0.55f, 0.80f, 1.00f), // 4 發動模式（淺藍）
        FLinearColor(0.38f, 0.88f, 0.48f), // 5 效果標示（綠）
        FLinearColor(0.75f, 0.75f, 0.75f), // 6 執行時機（灰）
        FLinearColor(1.00f, 0.88f, 0.28f), // 7 變數（黃）
        FLinearColor(1.00f, 0.65f, 0.20f), // 8 列表（深橙）
        FLinearColor(0.55f, 0.80f, 1.00f), // 9 實體（淺藍）
        FLinearColor(0.80f, 0.38f, 1.00f), // 10 廣播（紫）
        FLinearColor(0.95f, 0.65f, 0.95f), // 11 計數器（淺紫）
        FLinearColor(0.95f, 0.65f, 0.95f), // 12 統計（淺紫）
        FLinearColor(0.30f, 0.88f, 0.80f), // 13 向量（青綠）
        FLinearColor(0.90f, 0.30f, 0.30f), // 14 攔截（深紅）
        FLinearColor(0.55f, 0.65f, 0.75f), // 15 快照（藍灰）
    };
    return Colors[FMath::Clamp(CatIndex, 0, 15)];
}

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
    BuildPalette();

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
    void (UBlockEditorWidget::*MainHandlers[3])() = {
        &UBlockEditorWidget::OnMainTab0Clicked,
        &UBlockEditorWidget::OnMainTab1Clicked,
        &UBlockEditorWidget::OnMainTab2Clicked,
    };
    for (int32 i = 0; i < 3; ++i)
    {
        UButton* Btn = WidgetTree->ConstructWidget<UButton>();
        UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
        Txt->SetText(FText::FromString(MainTabNames[i]));
        Txt->SetJustification(ETextJustify::Center);
        Btn->AddChild(Txt);
        Btn->OnClicked.AddDynamic(this, MainHandlers[i]);
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

    USizeBox* SubTabSize = WidgetTree->ConstructWidget<USizeBox>();
    SubTabSize->SetWidthOverride(36.f);
    SubTabColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    SubTabSize->SetContent(SubTabColumn);
    if (UHorizontalBoxSlot* S = PaletteBody->AddChildToHorizontalBox(SubTabSize))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

    UScrollBox* ContentScroll = WidgetTree->ConstructWidget<UScrollBox>();
    PaletteContentList = WidgetTree->ConstructWidget<UVerticalBox>();
    ContentScroll->AddChild(PaletteContentList);
    if (UHorizontalBoxSlot* S = PaletteBody->AddChildToHorizontalBox(ContentScroll))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

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

    static void (UBlockEditorWidget::*SubHandlers[11])() = {
        &UBlockEditorWidget::OnSubTab0Clicked,  &UBlockEditorWidget::OnSubTab1Clicked,
        &UBlockEditorWidget::OnSubTab2Clicked,  &UBlockEditorWidget::OnSubTab3Clicked,
        &UBlockEditorWidget::OnSubTab4Clicked,  &UBlockEditorWidget::OnSubTab5Clicked,
        &UBlockEditorWidget::OnSubTab6Clicked,  &UBlockEditorWidget::OnSubTab7Clicked,
        &UBlockEditorWidget::OnSubTab8Clicked,  &UBlockEditorWidget::OnSubTab9Clicked,
        &UBlockEditorWidget::OnSubTab10Clicked,
    };

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
        Btn->OnClicked.AddDynamic(this, SubHandlers[i]);
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
            const bool bLocked = T.RequiredPlayerLevel > PlayerLevel;
            const FString LockTag = bLocked ? FString::Printf(TEXT("  🔒LV%d"), T.RequiredPlayerLevel) : TEXT("");
            const FText Label = FText::FromString(FString::Printf(TEXT("  %s%s  %dpt"),
                *T.DisplayName.ToString(), *LockTag, T.BaseAbilityPointCost));

            UPaletteItemWidget* Item = CreateWidget<UPaletteItemWidget>(this);
            const FName TotemId = T.TotemId;
            Item->Setup(Label, TotemTypeColor(T.Type), bLocked,
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
        const UEnum* BlockTypeEnum = StaticEnum<EBlockType>();

        for (int32 ci : CatIdx)
        {
            const FCat& Cat = Cats[ci];
            UTextBlock* CatLabel = WidgetTree->ConstructWidget<UTextBlock>();
            CatLabel->SetText(FText::FromString(Cat.Label));
            CatLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.50f, 0.50f, 0.62f)));
            PaletteContentList->AddChild(CatLabel);

            const FLinearColor Color = BlockCategoryColor(ci);
            for (EBlockType BT : Cat.Types)
            {
                // Phase 4 前的暫用顯示名：直接用 UENUM 識別字串（英文）；Phase 4 建立
                // FBlockUIDescriptor 集中表時補上逐型別中文名（對應 Godot ScratchCanvas._descs）。
                const FString Name = BlockTypeEnum->GetNameStringByValue(static_cast<int64>(BT));
                UPaletteItemWidget* Item = CreateWidget<UPaletteItemWidget>(this);
                Item->Setup(FText::FromString(TEXT("  ") + Name), Color, false,
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

void UBlockEditorWidget::RebuildList()
{
    if (!CenterList) return;
    CenterList->ClearChildren();
    if (!CurrentBlocks) return;

    UBlockCardWidget::BuildBlockList(this, CenterList, *CurrentBlocks, 0, [this]() { RebuildList(); });
}
