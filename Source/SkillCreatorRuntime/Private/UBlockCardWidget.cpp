#include "UBlockCardWidget.h"
#include "FBlockNode.h"
#include "Instruction.h"
#include "TotemLibrary.h"
#include "ManaTypeRegistry.h"
#include "UCardDragHandleWidget.h"
#include "UBlockDropZoneWidget.h"
#include "BlockUIDescriptor.h"
#include "UParamControlWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "SlateBrushHelpers.h"

// ── 卡片顏色/名稱（對應 Godot ScratchCanvas.BlockColor/BlockName）─────────────────
// 一般積木走 Phase 4 的 FBlockUIRegistry 集中表（逐型別精確色+中文名）；
// Totem/Engraving 因為顏色/名稱取決於積木實例攜帶的子型別資料，仍在此特殊處理。

static FLinearColor TotemTypeColorOf(ETotemType T)
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
        default:                       return FLinearColor(0.28f, 0.78f, 0.88f); // Totem 預設色（Godot CTotem）
    }
}

static FLinearColor EngraveColorOf(EEngraveColor C)
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
        default:                       return FLinearColor(0.72f, 0.55f, 0.92f); // Engraving 預設色（Godot CEngrave）
    }
}

// 對應 Godot ScratchCanvas.BlockName(BlockNode) / BlockColor(BlockType)（ScratchCanvas.cs:857-880）
static FText  GetBlockDisplayName(const FBlockNode& B);
static FLinearColor GetBlockDisplayColor(const FBlockNode& B);

static FText GetBlockDisplayName(const FBlockNode& B)
{
    if (B.Type == EBlockType::Totem)
    {
        const FInstancedStruct* IS = B.Params.Find(TEXT("args"));
        if (const FTotemBlockArgs* Args = IS ? IS->GetPtr<FTotemBlockArgs>() : nullptr)
        {
            if (Args->TotemId.ToString().Equals(TEXT("custom"), ESearchCase::IgnoreCase))
                return FText::FromString(TEXT("▸ ") + (Args->CustomName.IsEmpty() ? TEXT("自定義") : Args->CustomName.ToString()));
            if (const FTotemData* Data = FTotemLibrary::FindTotem(Args->TotemId))
                return FText::FromString(TEXT("▸ ") + Data->DisplayName.ToString());
        }
        return FText::FromString(TEXT("▸ ?"));
    }
    if (B.Type == EBlockType::Engraving)
    {
        const FInstancedStruct* IS = B.Params.Find(TEXT("args"));
        if (const FEngravingBlockArgs* Args = IS ? IS->GetPtr<FEngravingBlockArgs>() : nullptr)
            if (const FEngraveData* Data = FTotemLibrary::FindEngraving(Args->EngraveId))
                return FText::FromString(TEXT("◆ ") + Data->DisplayName.ToString());
        return FText::FromString(TEXT("◆ ?"));
    }
    // Phase 4：FBlockUIRegistry 集中表精確中文名（取代 Phase 2/3 的英文 UENUM 名稱）
    return FBlockUIRegistry::Get(B.Type).DisplayName;
}

static FLinearColor GetBlockDisplayColor(const FBlockNode& B)
{
    if (B.Type == EBlockType::Totem)
    {
        const FInstancedStruct* IS = B.Params.Find(TEXT("args"));
        if (const FTotemBlockArgs* Args = IS ? IS->GetPtr<FTotemBlockArgs>() : nullptr)
            return TotemTypeColorOf(Args->TotemType);
        return TotemTypeColorOf(ETotemType::Custom);
    }
    if (B.Type == EBlockType::Engraving)
    {
        const FInstancedStruct* IS = B.Params.Find(TEXT("args"));
        if (const FEngravingBlockArgs* Args = IS ? IS->GetPtr<FEngravingBlockArgs>() : nullptr)
            if (const FEngraveData* Data = FTotemLibrary::FindEngraving(Args->EngraveId))
                return EngraveColorOf(Data->Color);
        return EngraveColorOf(EEngraveColor::Action);
    }
    // Phase 4：FBlockUIRegistry 集中表精確色（取代 Phase 2/3 的「每類別一色」近似版）
    return FBlockUIRegistry::Get(B.Type).Color;
}

// ── 巢狀分支規格（對應 Godot BuildBlockCard 內各 BlockType 的分支標籤）───────────────

enum class EBranchSlot { Then, Else, Loop };

static void GetBranchSlots(EBlockType Type, TArray<TPair<EBranchSlot, FString>>& Out)
{
    switch (Type)
    {
        case EBlockType::If:
            Out.Add({ EBranchSlot::Then, TEXT("成立時執行") });
            Out.Add({ EBranchSlot::Else, TEXT("不成立時執行") });
            break;
        case EBlockType::Evaluate:
            Out.Add({ EBranchSlot::Then, TEXT("條件成立時執行") });
            break;
        case EBlockType::RepeatN:
        case EBlockType::RepeatWhile:
        case EBlockType::ForEachNearby:
            Out.Add({ EBranchSlot::Loop, TEXT("每輪執行") });
            break;
        case EBlockType::RandomChoice:
            Out.Add({ EBranchSlot::Then, TEXT("選項 A（50%）") });
            Out.Add({ EBranchSlot::Else, TEXT("選項 B（50%）") });
            break;
        case EBlockType::AlternateTrigger:
            Out.Add({ EBranchSlot::Then, TEXT("偶數次執行") });
            Out.Add({ EBranchSlot::Else, TEXT("奇數次執行") });
            break;
        case EBlockType::TaskCounterOnReach:
            Out.Add({ EBranchSlot::Then, TEXT("到達時執行（僅一次）") });
            break;
        case EBlockType::SinglePulse:
            Out.Add({ EBranchSlot::Then, TEXT("條件首次成立時執行") });
            break;
        default: break;
    }
}

static TArray<TUniquePtr<FBlockNode>>* GetBranchArray(FBlockNode& Block, EBranchSlot Slot)
{
    switch (Slot)
    {
        case EBranchSlot::Then: return &Block.ThenBranch;
        case EBranchSlot::Else: return &Block.ElseBranch;
        case EBranchSlot::Loop: return &Block.LoopBody;
        default:                return nullptr;
    }
}

// ══════════════════════════════════════════════════════════════════

void UBlockCardWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

FReply UBlockCardWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // 對應 Godot ScratchCanvas.BlockDoubleClicked（AbilityEditorUI.cs:918-928）
    if (Block && OnDoubleClicked)
    {
        OnDoubleClicked(Block);
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

void UBlockCardWidget::Setup(FBlockNode* InBlock, TArray<TUniquePtr<FBlockNode>>* InParentList, int32 InIndex,
                              int32 InIndent, TFunction<void()> InOnChanged,
                              TFunction<void(FBlockNode*)> InOnDoubleClicked)
{
    Block           = InBlock;
    ParentList      = InParentList;
    IndexInParent   = InIndex;
    Indent          = InIndent;
    OnChanged       = MoveTemp(InOnChanged);
    OnDoubleClicked = MoveTemp(InOnDoubleClicked);

    UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>();
    WidgetTree->RootWidget = Outer;

    BuildCardRow(Outer);
    BuildNestedBranches(Outer);
}

void UBlockCardWidget::BuildCardRow(UVerticalBox* Outer)
{
    if (!Block) return;

    // 卡片本體（對應 Godot ScratchCanvas.cs:103-115：32px 高、圓角、深色背景）
    USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
    CardSize->SetHeightOverride(32.f);
    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrush(MakeSolidBrush(FLinearColor(0.17f, 0.17f, 0.21f, 1.f)));
    CardSize->SetContent(Card);

    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    Card->SetContent(Row);

    // 縮排（14px/層，Godot ScratchCanvas.cs:125）
    if (Indent > 0)
    {
        USizeBox* Spacer = WidgetTree->ConstructWidget<USizeBox>();
        Spacer->SetWidthOverride(Indent * 14.f);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Spacer))
            S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    }

    // 拖拉手把（8px，對應 Godot DragHandle）
    const FLinearColor Color = GetBlockDisplayColor(*Block);
    USizeBox* HandleSize = WidgetTree->ConstructWidget<USizeBox>();
    HandleSize->SetWidthOverride(8.f);
    UCardDragHandleWidget* Handle = CreateWidget<UCardDragHandleWidget>(this);
    Handle->Setup(Color, ParentList, IndexInParent);
    HandleSize->SetContent(Handle);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(HandleSize))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

    // 名稱 label（Godot ScratchCanvas.cs:131-136）
    UTextBlock* NameLbl = WidgetTree->ConstructWidget<UTextBlock>();
    NameLbl->SetText(GetBlockDisplayName(*Block));
    NameLbl->SetColorAndOpacity(FSlateColor(Color));
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(NameLbl))
    {
        S->SetPadding(FMargin(6.f, 0.f));
        S->SetVerticalAlignment(VAlign_Center);
    }

    // 參數控制項（對應 Godot ScratchCanvas.AddParams，ScratchCanvas.cs:290-305）
    if (Block->Type == EBlockType::Totem)
    {
        // Totem 特殊 UI：自訂名稱輸入（totemId=="custom" 才顯示）+ MP 類型輸入
        // （非被動才顯示）。對應 Godot ScratchCanvas.cs:818-842。
        FInstancedStruct& IS = Block->Params.FindOrAdd(TEXT("args"));
        if (!IS.IsValid() || !IS.GetPtr<FTotemBlockArgs>())
            IS = FInstancedStruct::Make<FTotemBlockArgs>(FTotemBlockArgs{});
        if (FTotemBlockArgs* Args = IS.GetMutablePtr<FTotemBlockArgs>())
        {
            if (Args->TotemId.ToString().Equals(TEXT("custom"), ESearchCase::IgnoreCase))
            {
                UParamTextEditWidget* NameEdit = CreateWidget<UParamTextEditWidget>(this);
                NameEdit->Setup(Args->CustomName.ToString(), FText::FromString(TEXT("技能因子名稱")), 80.f,
                    [Args](const FString& T) { Args->CustomName = FText::FromString(T); });
                if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(NameEdit))
                    S->SetVerticalAlignment(VAlign_Center);
            }
            if (Args->TotemType != ETotemType::Passive)
            {
                // 對應 Godot ScratchCanvas.cs:824-841：下拉選單列出 ManaTypeRegistry 全部類型
                // （原先用純文字框讓使用者手打 key，看不出有哪些選項可選，改回真正的下拉選單）
                UParamTinyLabelWidget* ManaLbl = CreateWidget<UParamTinyLabelWidget>(this);
                ManaLbl->Setup(FText::FromString(TEXT("MP 類型")));
                if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(ManaLbl))
                    S->SetVerticalAlignment(VAlign_Center);

                TArray<FString> Values{ FString() };
                TArray<FString> Labels{ TEXT("─") };
                for (const FManaType* MT : FManaTypeRegistry::Get().GetSortedForHud())
                {
                    Values.Add(MT->Key.ToString());
                    Labels.Add(MT->DisplayName.ToString());
                }

                UParamDropdownWidget* ManaDrop = CreateWidget<UParamDropdownWidget>(this);
                ManaDrop->Setup(Values, Labels, Args->ManaTypeKey.ToString(), 96.f,
                    [Args](const FString& T) { Args->ManaTypeKey = FName(*T); });
                if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(ManaDrop))
                    S->SetVerticalAlignment(VAlign_Center);
            }
        }
    }
    else if (Block->Type == EBlockType::Engraving)
    {
        // Engraving 特殊 UI：投入點數（對應 Godot ScratchCanvas.cs:1003-1004「投入 N pt」）
        FInstancedStruct& IS = Block->Params.FindOrAdd(TEXT("args"));
        if (!IS.IsValid() || !IS.GetPtr<FEngravingBlockArgs>())
            IS = FInstancedStruct::Make<FEngravingBlockArgs>(FEngravingBlockArgs{});
        if (FEngravingBlockArgs* Args = IS.GetMutablePtr<FEngravingBlockArgs>())
        {
            UParamTinyLabelWidget* Lbl = CreateWidget<UParamTinyLabelWidget>(this);
            Lbl->Setup(FText::FromString(TEXT("投入")));
            if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Lbl))
                S->SetVerticalAlignment(VAlign_Center);

            UParamSpinWidget* PtsSpin = CreateWidget<UParamSpinWidget>(this);
            PtsSpin->Setup(Args->Points, 0.f, 100.f, 1.f, 36.f, [Args](float V) { Args->Points = V; });
            if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(PtsSpin))
                S->SetVerticalAlignment(VAlign_Center);

            UParamTinyLabelWidget* PtLbl = CreateWidget<UParamTinyLabelWidget>(this);
            PtLbl->Setup(FText::FromString(TEXT("pt")));
            if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(PtLbl))
                S->SetVerticalAlignment(VAlign_Center);
        }
    }
    else
    {
        const FBlockUIDescriptor& Desc = FBlockUIRegistry::Get(Block->Type);
        if (Desc.BuildParamsUI)
            Desc.BuildParamsUI(this, Row, *Block, OnChanged);
    }

    // 彈性間隔，把刪除鈕推到最右
    UBorder* Flex = WidgetTree->ConstructWidget<UBorder>();
    Flex->SetBrush(MakeSolidBrush(FLinearColor::Transparent));
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Flex))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    // 刪除鈕（Godot MiniBtn "✕"，ScratchCanvas.cs:142-146）
    UButton* Del = WidgetTree->ConstructWidget<UButton>();
    Del->SetBackgroundColor(FLinearColor(0.5f, 0.15f, 0.15f, 1.f));
    {
        UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
        Txt->SetText(FText::FromString(TEXT("✕")));
        Del->AddChild(Txt);
    }
    Del->OnClicked.AddDynamic(this, &UBlockCardWidget::OnDeleteClicked);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Del))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        S->SetVerticalAlignment(VAlign_Center);
    }

    if (UVerticalBoxSlot* S = Outer->AddChildToVerticalBox(CardSize))
        S->SetHorizontalAlignment(HAlign_Fill);
}

void UBlockCardWidget::BuildNestedBranches(UVerticalBox* Outer)
{
    if (!Block) return;

    TArray<TPair<EBranchSlot, FString>> Slots;
    GetBranchSlots(Block->Type, Slots);

    for (int32 i = 0; i < Slots.Num() && i < 2; ++i)
    {
        TArray<TUniquePtr<FBlockNode>>* Branch = GetBranchArray(*Block, Slots[i].Key);
        if (!Branch) continue;
        if (UVerticalBoxSlot* S = Outer->AddChildToVerticalBox(BuildBranchPanel(Slots[i].Value, *Branch, i)))
            S->SetHorizontalAlignment(HAlign_Fill);
    }
}

UWidget* UBlockCardWidget::BuildBranchPanel(const FString& Label, TArray<TUniquePtr<FBlockNode>>& BranchBlocks, int32 SlotIndex)
{
    // 綠色邊框容器（對應 Godot BuildBranch，ScratchCanvas.cs:198-256）
    UBorder* Wrap = WidgetTree->ConstructWidget<UBorder>();
    Wrap->SetBrush(MakeSolidBrush(FLinearColor(0.13f, 0.15f, 0.13f, 1.f)));
    Wrap->SetPadding(FMargin((Indent + 1) * 14.f, 2.f, 2.f, 2.f));

    UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>();
    Wrap->SetContent(VBox);

    UTextBlock* HdrLbl = WidgetTree->ConstructWidget<UTextBlock>();
    HdrLbl->SetText(FText::FromString(Label));
    HdrLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.40f, 0.70f, 0.40f)));
    VBox->AddChildToVerticalBox(HdrLbl);

    BuildBlockList(this, VBox, BranchBlocks, Indent + 1, OnChanged, OnDoubleClicked);

    // 「＋ 加入積木」按鈕（對應 ScratchCanvas.cs:233-247，預設新增 InvokeTotem 佔位積木）
    UButton* AddBtn = WidgetTree->ConstructWidget<UButton>();
    AddBtn->SetBackgroundColor(FLinearColor::Transparent);
    {
        UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
        Txt->SetText(FText::FromString(TEXT("＋  加入積木")));
        Txt->SetColorAndOpacity(FSlateColor(FLinearColor(0.40f, 0.65f, 0.40f)));
        AddBtn->AddChild(Txt);
    }
    AddBranchTarget[FMath::Clamp(SlotIndex, 0, 1)] = &BranchBlocks;
    if (SlotIndex == 0)      AddBtn->OnClicked.AddDynamic(this, &UBlockCardWidget::OnAddToBranch0Clicked);
    else if (SlotIndex == 1) AddBtn->OnClicked.AddDynamic(this, &UBlockCardWidget::OnAddToBranch1Clicked);
    VBox->AddChildToVerticalBox(AddBtn);

    return Wrap;
}

void UBlockCardWidget::OnDeleteClicked()
{
    if (!ParentList || !ParentList->IsValidIndex(IndexInParent)) return;
    ParentList->RemoveAt(IndexInParent);
    if (OnChanged) OnChanged();
}

void UBlockCardWidget::AddDefaultBlockTo(TArray<TUniquePtr<FBlockNode>>* Target)
{
    // 對應 Godot ScratchCanvas.cs:233-247：分支內「＋加入積木」預設新增 InvokeTotem 佔位積木
    if (!Target) return;
    TUniquePtr<FBlockNode> NewBlock = MakeUnique<FBlockNode>();
    NewBlock->Type = EBlockType::InvokeTotem;
    Target->Add(MoveTemp(NewBlock));
    if (OnChanged) OnChanged();
}

void UBlockCardWidget::OnAddToBranch0Clicked() { AddDefaultBlockTo(AddBranchTarget[0]); }
void UBlockCardWidget::OnAddToBranch1Clicked() { AddDefaultBlockTo(AddBranchTarget[1]); }

void UBlockCardWidget::BuildBlockList(UWidget* WidgetContext, UVerticalBox* Container,
                                       TArray<TUniquePtr<FBlockNode>>& Blocks, int32 Indent,
                                       const TFunction<void()>& OnChanged,
                                       const TFunction<void(FBlockNode*)>& OnDoubleClicked)
{
    if (!WidgetContext || !Container) return;

    auto MakeDropZone = [&](int32 InsertAt) -> UWidget*
    {
        UBlockDropZoneWidget* Zone = CreateWidget<UBlockDropZoneWidget>(WidgetContext, UBlockDropZoneWidget::StaticClass());
        Zone->Setup(&Blocks, InsertAt, OnChanged);
        return Zone;
    };

    Container->AddChild(MakeDropZone(0));

    for (int32 i = 0; i < Blocks.Num(); ++i)
    {
        UBlockCardWidget* Card = CreateWidget<UBlockCardWidget>(WidgetContext, UBlockCardWidget::StaticClass());
        Card->Setup(Blocks[i].Get(), &Blocks, i, Indent, OnChanged, OnDoubleClicked);
        if (UVerticalBoxSlot* S = Container->AddChildToVerticalBox(Card))
            S->SetHorizontalAlignment(HAlign_Fill);

        Container->AddChild(MakeDropZone(i + 1));
    }
}
