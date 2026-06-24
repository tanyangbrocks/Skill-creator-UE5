#include "USpellListWidget.h"
#include "USpellCircleWidget.h"
#include "ASkillCreatorCharacter.h"
#include "USpellCaster.h"
#include "SpellArray.h"
#include "SpellLoadout.h"
#include "SpellGroup.h"
#include "SpellDescriptionGenerator.h"
#include "ElementType.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "SlateBrushHelpers.h"

static constexpr float GCircleSize    = 260.f;
static constexpr float GCircleSpacing =  22.f;
static constexpr int32 GMaxGroups     =   5;

// ─── 工具 ────────────────────────────────────────────────────────────────────

ASkillCreatorCharacter* USpellListWidget::GetOwnerCharacter() const
{
    APlayerController* PC = GetOwningPlayer();
    return PC ? Cast<ASkillCreatorCharacter>(PC->GetPawn()) : nullptr;
}

bool USpellListWidget::IsAtLimit(const FSpellLoadout& Loadout) const
{
    return CountActiveNamed(Loadout) >= FSpellLoadout::MaxSlots
        && Loadout.PassiveSpells.Num() >= FSpellLoadout::MaxPassiveSlots;
}

int32 USpellListWidget::CountActiveNamed(const FSpellLoadout& Loadout) const
{
    int32 Count = 0;
    for (int32 i = 0; i < FSpellLoadout::MaxSlots; ++i)
        if (Loadout.Slots.IsValidIndex(i) && Loadout.Slots[i].IsValid()) ++Count;
    return Count;
}

// ─── 初始化 ──────────────────────────────────────────────────────────────────

void USpellListWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();
    RefreshSpellList();
}

// ─── BuildLayout ─────────────────────────────────────────────────────────────
// 對應 Godot SpellListUI.cs:62-243 BuildUI()

void USpellListWidget::BuildLayout()
{
    // Root：CanvasPanel（浮動 Tooltip 需要 CanvasPanelSlot 定位）
    RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = RootCanvas;

    // ── 整體背景 + 主 VBox ────────────────────────────────────────────────────
    // 對應 Godot SpellListUI.cs:64-71 ColorRect bg(0.08,0.08,0.12,0.97) + VBoxContainer
    UBorder* MainBg = WidgetTree->ConstructWidget<UBorder>();
    MainBg->SetBrush(MakeSolidBrush(FLinearColor(0.08f, 0.08f, 0.12f, 0.97f)));
    MainBg->SetPadding(FMargin(0.f));
    RootCanvas->AddChild(MainBg);
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(MainBg->Slot))
    {
        S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
        S->SetOffsets(FMargin(0.f));
    }

    UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>();
    MainBg->AddChild(MainVBox);

    // ── 標題列（Header）──────────────────────────────────────────────────────
    // 對應 Godot SpellListUI.cs:73-122 header PanelContainer + HBoxContainer
    UBorder* Header = WidgetTree->ConstructWidget<UBorder>();
    Header->SetBrush(MakeSolidBrush(FLinearColor(0.12f, 0.12f, 0.18f, 1.f)));
    Header->SetPadding(FMargin(0.f));
    {
        USizeBox* HeaderSize = WidgetTree->ConstructWidget<USizeBox>();
        HeaderSize->SetMinDesiredHeight(58.f);
        HeaderSize->SetContent(Header);
        if (UVerticalBoxSlot* S = MainVBox->AddChildToVerticalBox(HeaderSize))
            S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    }

    UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    Header->AddChild(HeaderRow);

    // Spacer 24px
    {
        USizeBox* Sp = WidgetTree->ConstructWidget<USizeBox>();
        Sp->SetWidthOverride(24.f);
        if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(Sp))
            S->SetVerticalAlignment(VAlign_Center);
    }

    // 標題 "技能創建空間"（Godot SpellListUI.cs:88-92）
    {
        UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
        Title->SetText(FText::FromString(TEXT("技能創建空間")));
        FSlateFontInfo F = Title->GetFont(); F.Size = 22; Title->SetFont(F);
        Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(Title))
            S->SetVerticalAlignment(VAlign_Center);
    }

    // Spacer 16px
    {
        USizeBox* Sp = WidgetTree->ConstructWidget<USizeBox>();
        Sp->SetWidthOverride(16.f);
        if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(Sp))
            S->SetVerticalAlignment(VAlign_Center);
    }

    // 技能組圓點（1~5 數字按鈕，Godot SpellListUI.cs:94-111）
    {
        UHorizontalBox* DotBox = WidgetTree->ConstructWidget<UHorizontalBox>();
        GroupDotButtons.SetNum(GMaxGroups);
        for (int32 gi = 0; gi < GMaxGroups; ++gi)
        {
            UButton* Dot = WidgetTree->ConstructWidget<UButton>();
            Dot->SetBackgroundColor(FLinearColor(0.18f, 0.18f, 0.22f, 1.f)); // 預設灰（非作用中）

            UTextBlock* Num = WidgetTree->ConstructWidget<UTextBlock>();
            Num->SetText(FText::FromString(FString::FromInt(gi + 1)));
            FSlateFontInfo NF = Num->GetFont(); NF.Size = 13; Num->SetFont(NF);
            Num->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.85f)));
            Dot->AddChild(Num);

            // AddDynamic 是巨集，不能用陣列變數（會把 "GroupHandlers[gi]" 當字面字串導致 assert）
            switch (gi)
            {
            case 0: Dot->OnClicked.AddDynamic(this, &USpellListWidget::OnGroupBtn0Clicked); break;
            case 1: Dot->OnClicked.AddDynamic(this, &USpellListWidget::OnGroupBtn1Clicked); break;
            case 2: Dot->OnClicked.AddDynamic(this, &USpellListWidget::OnGroupBtn2Clicked); break;
            case 3: Dot->OnClicked.AddDynamic(this, &USpellListWidget::OnGroupBtn3Clicked); break;
            case 4: Dot->OnClicked.AddDynamic(this, &USpellListWidget::OnGroupBtn4Clicked); break;
            default: break;
            }

            USizeBox* DotSize = WidgetTree->ConstructWidget<USizeBox>();
            DotSize->SetWidthOverride (28.f);
            DotSize->SetHeightOverride(28.f);
            DotSize->SetContent(Dot);

            if (UHorizontalBoxSlot* S = DotBox->AddChildToHorizontalBox(DotSize))
            {
                S->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
                S->SetVerticalAlignment(VAlign_Center);
            }
            GroupDotButtons[gi] = Dot;
        }
        if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(DotBox))
            S->SetVerticalAlignment(VAlign_Center);
    }

    // ExpandFill 彈性空白
    {
        USizeBox* Filler = WidgetTree->ConstructWidget<USizeBox>();
        if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(Filler))
        {
            S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            S->SetVerticalAlignment(VAlign_Center);
        }
    }

    // 提示文字 "點擊圓球編輯技能　E 關閉"（Godot SpellListUI.cs:115-119）
    {
        UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>();
        Hint->SetText(FText::FromString(TEXT("點擊圓球編輯技能　E 關閉")));
        FSlateFontInfo HF = Hint->GetFont(); HF.Size = 13; Hint->SetFont(HF);
        Hint->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.65f)));
        if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(Hint))
            S->SetVerticalAlignment(VAlign_Center);
    }

    // Spacer 24px
    {
        USizeBox* Sp = WidgetTree->ConstructWidget<USizeBox>();
        Sp->SetWidthOverride(24.f);
        if (UHorizontalBoxSlot* S = HeaderRow->AddChildToHorizontalBox(Sp))
            S->SetVerticalAlignment(VAlign_Center);
    }

    // ── 圓球捲動區（CircleArea）──────────────────────────────────────────────
    // 對應 Godot SpellListUI.cs:124-163
    {
        UBorder* CircleAreaBg = WidgetTree->ConstructWidget<UBorder>();
        CircleAreaBg->SetBrush(MakeSolidBrush(FLinearColor(0.10f, 0.10f, 0.14f, 1.f)));
        CircleAreaBg->SetPadding(FMargin(0.f));
        if (UVerticalBoxSlot* S = MainVBox->AddChildToVerticalBox(CircleAreaBg))
        {
            S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            S->SetVerticalAlignment(VAlign_Fill);
        }

        // Overlay 把 ScrollBox 垂直置中（對應 Godot inner VBox + ExpandFill 上下）
        UOverlay* CenterOverlay = WidgetTree->ConstructWidget<UOverlay>();
        if (UBorderSlot* BS = Cast<UBorderSlot>(CircleAreaBg->AddChild(CenterOverlay)))
        {
            BS->SetHorizontalAlignment(HAlign_Fill);
            BS->SetVerticalAlignment(VAlign_Fill);
        }

        // ScrollBox 水平捲動，高度鎖定 CircleSize+44
        UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
        Scroll->SetOrientation(EOrientation::Orient_Horizontal);
        Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);

        USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>();
        ScrollSize->SetMinDesiredHeight(GCircleSize + 44.f);
        ScrollSize->SetContent(Scroll);

        if (UOverlaySlot* OS = CenterOverlay->AddChildToOverlay(ScrollSize))
        {
            OS->SetHorizontalAlignment(HAlign_Fill);
            OS->SetVerticalAlignment(VAlign_Center);
        }

        // CircleRow（RebuildCircles 每次清空再填）
        CircleRow = WidgetTree->ConstructWidget<UHorizontalBox>();
        if (UScrollBoxSlot* SBS = Cast<UScrollBoxSlot>(Scroll->AddChild(CircleRow)))
            SBS->SetPadding(FMargin(28.f, 0.f, 28.f, 0.f));
    }

    // ── 底部欄（Footer）──────────────────────────────────────────────────────
    // 對應 Godot SpellListUI.cs:166-200
    {
        UBorder* Footer = WidgetTree->ConstructWidget<UBorder>();
        Footer->SetBrush(MakeSolidBrush(FLinearColor(0.12f, 0.12f, 0.18f, 1.f)));
        Footer->SetPadding(FMargin(0.f));

        USizeBox* FooterSize = WidgetTree->ConstructWidget<USizeBox>();
        FooterSize->SetMinDesiredHeight(38.f);
        FooterSize->SetContent(Footer);
        if (UVerticalBoxSlot* S = MainVBox->AddChildToVerticalBox(FooterSize))
            S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

        UHorizontalBox* FootRow = WidgetTree->ConstructWidget<UHorizontalBox>();
        Footer->AddChild(FootRow);

        // Spacer 24px
        {
            USizeBox* Sp = WidgetTree->ConstructWidget<USizeBox>();
            Sp->SetWidthOverride(24.f);
            if (UHorizontalBoxSlot* S = FootRow->AddChildToHorizontalBox(Sp))
                S->SetVerticalAlignment(VAlign_Center);
        }

        // "主動上限 N　被動上限 M"（Godot SpellListUI.cs:181-188）
        {
            UTextBlock* CapLbl = WidgetTree->ConstructWidget<UTextBlock>();
            CapLbl->SetText(FText::FromString(
                FString::Printf(TEXT("主動上限 %d　被動上限 %d"),
                                FSpellLoadout::MaxSlots, FSpellLoadout::MaxPassiveSlots)));
            FSlateFontInfo CF = CapLbl->GetFont(); CF.Size = 13; CapLbl->SetFont(CF);
            CapLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.45f, 0.55f)));
            if (UHorizontalBoxSlot* S = FootRow->AddChildToHorizontalBox(CapLbl))
                S->SetVerticalAlignment(VAlign_Center);
        }

        // ExpandFill 彈性空白
        {
            USizeBox* Filler = WidgetTree->ConstructWidget<USizeBox>();
            if (UHorizontalBoxSlot* S = FootRow->AddChildToHorizontalBox(Filler))
            {
                S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
                S->SetVerticalAlignment(VAlign_Center);
            }
        }

        // 暫時訊息 Label（Godot SpellListUI.cs:192-198 _msgLabel，3 秒後消失）
        {
            MsgLabel = WidgetTree->ConstructWidget<UTextBlock>();
            MsgLabel->SetText(FText::GetEmpty());
            FSlateFontInfo MF = MsgLabel->GetFont(); MF.Size = 13; MsgLabel->SetFont(MF);
            MsgLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.3f)));
            MsgLabel->SetVisibility(ESlateVisibility::Hidden);
            if (UHorizontalBoxSlot* S = FootRow->AddChildToHorizontalBox(MsgLabel))
                S->SetVerticalAlignment(VAlign_Center);
        }

        // Spacer 24px
        {
            USizeBox* Sp = WidgetTree->ConstructWidget<USizeBox>();
            Sp->SetWidthOverride(24.f);
            if (UHorizontalBoxSlot* S = FootRow->AddChildToHorizontalBox(Sp))
                S->SetVerticalAlignment(VAlign_Center);
        }
    }

    // ── 浮動 Tooltip（Godot SpellListUI.cs:215-243）────────────────────────────
    // 靠 NativeTick 在 CanvasPanel 內追蹤滑鼠位置
    {
        TooltipPanel = WidgetTree->ConstructWidget<UBorder>();
        TooltipPanel->SetBrush(MakeSolidBrush(FLinearColor(0.10f, 0.10f, 0.15f, 0.96f)));
        TooltipPanel->SetPadding(FMargin(12.f, 8.f));
        TooltipPanel->SetVisibility(ESlateVisibility::Hidden);
        RootCanvas->AddChild(TooltipPanel);
        if (UCanvasPanelSlot* TS = Cast<UCanvasPanelSlot>(TooltipPanel->Slot))
        {
            TS->SetAutoSize(true);
            TS->SetZOrder(20);
            TS->SetPosition(FVector2D(-9999.f, -9999.f));
        }

        TooltipLabel = WidgetTree->ConstructWidget<UTextBlock>();
        TooltipLabel->SetAutoWrapText(true);
        FSlateFontInfo TF = TooltipLabel->GetFont(); TF.Size = 13; TooltipLabel->SetFont(TF);
        TooltipLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        TooltipPanel->AddChild(TooltipLabel);
    }
}

// ─── RebuildCircles ───────────────────────────────────────────────────────────
// 對應 Godot SpellListUI.cs:249-297 RebuildCircles()
// 每次 RefreshSpellList 時全清再重建，只渲染「真的存在」的技能

void USpellListWidget::RebuildCircles()
{
    if (!CircleRow) return;
    CircleRow->ClearChildren();

    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    if (!Char || !Char->SpellCasterComp) return;

    const FSpellLoadout& Loadout = Char->SpellCasterComp->SpellGroups.GetActiveLoadout();

    // ── 主動技能圓球（Godot SpellListUI.cs:261-273）──────────────────────────
    for (int32 i = 0; i < FSpellLoadout::MaxSlots; ++i)
    {
        if (!Loadout.Slots.IsValidIndex(i)) continue;
        const FSpellArray& Spell = Loadout.Slots[i];
        if (!Spell.IsValid()) continue;

        FString Prose   = FSpellDescriptionGenerator::GenerateProse(Spell);
        FString TipText = FString::Printf(TEXT("%s\n\n%s"), *Spell.Name, *Prose);

        USpellCircleWidget* Circle = CreateWidget<USpellCircleWidget>(
            GetOwningPlayer(), USpellCircleWidget::StaticClass());
        if (!Circle) continue;

        int32 CapturedIdx = i;
        Circle->SetupSpell(
            Spell.Name, TipText,
            /*bIsPassive=*/false, /*bIsOverLimit=*/false,
            [this, CapturedIdx]() { OnSlotClicked.ExecuteIfBound(CapturedIdx); },
            [this](const FString& T)  { ShowTooltip(T); },
            [this]()                   { HideTooltip();  }
        );

        if (UHorizontalBoxSlot* S = CircleRow->AddChildToHorizontalBox(Circle))
            S->SetPadding(FMargin(0.f, 0.f, GCircleSpacing, 0.f));
    }

    // ── 被動技能圓球（Godot SpellListUI.cs:275-287）──────────────────────────
    // 點擊被動圓球目前只顯示提示訊息（對應 Godot Stage 2 PassiveSpellClicked 未實作完整編輯）
    for (int32 pi = 0; pi < Loadout.PassiveSpells.Num(); ++pi)
    {
        const FSpellArray& Spell = Loadout.PassiveSpells[pi];
        if (!Spell.IsValid()) continue;

        FString Prose   = FSpellDescriptionGenerator::GenerateProse(Spell);
        FString TipText = FString::Printf(TEXT("%s\n\n%s"), *Spell.Name, *Prose);
        bool bOver = pi >= FSpellLoadout::MaxPassiveSlots;

        USpellCircleWidget* Circle = CreateWidget<USpellCircleWidget>(
            GetOwningPlayer(), USpellCircleWidget::StaticClass());
        if (!Circle) continue;

        Circle->SetupSpell(
            Spell.Name, TipText,
            /*bIsPassive=*/true, bOver,
            [this]() { ShowMsg(TEXT("被動技能暫不支援編輯")); },
            [this](const FString& T)  { ShowTooltip(T); },
            [this]()                   { HideTooltip();  }
        );

        if (UHorizontalBoxSlot* S = CircleRow->AddChildToHorizontalBox(Circle))
            S->SetPadding(FMargin(0.f, 0.f, GCircleSpacing, 0.f));
    }

    // ── 「+」新增圓球（Godot SpellListUI.cs:290-296）───────────────────────
    if (!IsAtLimit(Loadout))
    {
        USpellCircleWidget* AddCircle = CreateWidget<USpellCircleWidget>(
            GetOwningPlayer(), USpellCircleWidget::StaticClass());
        if (AddCircle)
        {
            AddCircle->SetupAdd([this]() { OnAddSpellRequested.ExecuteIfBound(); });
            CircleRow->AddChildToHorizontalBox(AddCircle);
        }
    }
}

// ─── RefreshGroupDots ─────────────────────────────────────────────────────────
// 對應 Godot SpellListUI.cs:48-56 RefreshGroupDots()（Modulate 亮藍 vs 灰）

void USpellListWidget::RefreshGroupDots()
{
    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    int32 Active = Char && Char->SpellCasterComp
                 ? Char->SpellCasterComp->SpellGroups.ActiveGroupIndex : 0;

    for (int32 gi = 0; gi < GMaxGroups; ++gi)
    {
        if (!GroupDotButtons.IsValidIndex(gi) || !GroupDotButtons[gi]) continue;
        GroupDotButtons[gi]->SetBackgroundColor(
            gi == Active
                ? FLinearColor(0.12f, 0.38f, 0.65f, 1.f)  // 亮藍（作用中，對應 Godot Color(0.40,0.85,1.00)）
                : FLinearColor(0.18f, 0.18f, 0.22f, 1.f)   // 灰（其他，對應 Godot Color(0.55,0.55,0.65)）
        );
    }
}

// ─── RefreshSpellList ─────────────────────────────────────────────────────────

void USpellListWidget::RefreshSpellList()
{
    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    if (!Char || !Char->SpellCasterComp) return;

    // 更新 CachedSlots（維持 GetCachedSlots() 公開 API 的資料正確性）
    const FSpellGroup&   Group   = Char->SpellCasterComp->SpellGroups;
    const FSpellLoadout& Loadout = Group.GetActiveLoadout();

    CachedSlots.Reset(FSpellLoadout::MaxSlots);
    for (int32 i = 0; i < FSpellLoadout::MaxSlots; ++i)
    {
        FSpellSlotDisplayInfo Info;
        Info.SlotIndex    = i;
        Info.bIsActiveSlot = (i == Loadout.ActiveIndex);
        if (Loadout.Slots.IsValidIndex(i))
        {
            const FSpellArray& S = Loadout.Slots[i];
            Info.bIsEmpty   = !S.IsValid();
            Info.bIsPassive = S.IsPassive();
            if (!Info.bIsEmpty)
            {
                Info.SpellName      = S.Name;
                Info.BaseMpCost     = S.BaseMpCost;
                Info.StructuredDesc = FSpellDescriptionGenerator::GenerateStructured(S);
            }
        }
        else { Info.bIsEmpty = true; }
        CachedSlots.Add(MoveTemp(Info));
    }

    RebuildCircles();
    RefreshGroupDots();
}

// ─── Public API ──────────────────────────────────────────────────────────────

int32 USpellListWidget::GetActiveGroupIndex() const
{
    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    return Char && Char->SpellCasterComp
         ? Char->SpellCasterComp->SpellGroups.ActiveGroupIndex : 0;
}

void USpellListWidget::SetActiveGroup(int32 GroupIndex)
{
    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    if (!Char || !Char->SpellCasterComp) return;
    Char->SpellCasterComp->SpellGroups.SetActiveGroup(GroupIndex);
    RefreshSpellList();
}

// ─── 5 個組別按鈕 UFUNCTION ──────────────────────────────────────────────────

void USpellListWidget::OnGroupBtn0Clicked() { SetActiveGroup(0); }
void USpellListWidget::OnGroupBtn1Clicked() { SetActiveGroup(1); }
void USpellListWidget::OnGroupBtn2Clicked() { SetActiveGroup(2); }
void USpellListWidget::OnGroupBtn3Clicked() { SetActiveGroup(3); }
void USpellListWidget::OnGroupBtn4Clicked() { SetActiveGroup(4); }

// ─── Tooltip ─────────────────────────────────────────────────────────────────

void USpellListWidget::ShowTooltip(const FString& Text)
{
    if (!TooltipPanel || !TooltipLabel) return;
    TooltipLabel->SetText(FText::FromString(Text));
    TooltipPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USpellListWidget::HideTooltip()
{
    if (TooltipPanel) TooltipPanel->SetVisibility(ESlateVisibility::Hidden);
}

// ─── ShowMsg ─────────────────────────────────────────────────────────────────
// 對應 Godot SpellListUI.cs:334-339 ShowMsg()（底部欄暫時訊息，3 秒後消失）

void USpellListWidget::ShowMsg(const FString& Msg)
{
    if (!MsgLabel) return;
    MsgLabel->SetText(FText::FromString(Msg));
    MsgLabel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    MsgTimer = 3.f;
}

// ─── NativeTick ──────────────────────────────────────────────────────────────
// Tooltip 位置跟滑鼠（對應 Godot SpellListUI.cs:341-358 _Process）
// 訊息計時器倒數

void USpellListWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Tooltip 跟隨滑鼠
    if (TooltipPanel && TooltipPanel->GetVisibility() == ESlateVisibility::HitTestInvisible)
    {
        if (APlayerController* PC = GetOwningPlayer())
        {
            float MX, MY;
            PC->GetMousePosition(MX, MY);
            if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(TooltipPanel->Slot))
                S->SetPosition(FVector2D(MX, MY) + FVector2D(14.f, 14.f));
        }
    }

    // 訊息倒數（對應 Godot SpellListUI.cs:342-348）
    if (MsgTimer > 0.f)
    {
        MsgTimer -= InDeltaTime;
        if (MsgTimer <= 0.f && MsgLabel)
            MsgLabel->SetVisibility(ESlateVisibility::Hidden);
    }
}
