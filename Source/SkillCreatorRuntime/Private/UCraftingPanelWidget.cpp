#include "UCraftingPanelWidget.h"
#include "UCraftingCardWidget.h"
#include "UInventoryComponent.h"
#include "RecipeRegistry.h"
#include "ItemRegistry.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "SlateBrushHelpers.h"

void UCraftingPanelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // Shift 展開的完整加工面板：可捲動圖卡列表，預設 Collapsed，由 ASkillCreatorHUD::ToggleCraftingPanel 控制。
    // 位置在提示圖卡（x=10, 44px 寬）右側（x=60），垂直置中。
    constexpr float PanelW = 200.f, PanelH = 280.f;

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CraftPanel"));
    Panel->SetBrush(MakeSolidBrush(FLinearColor(0.05f, 0.08f, 0.06f, 0.85f)));
    Panel->SetPadding(FMargin(6.f));
    Root->AddChild(Panel);
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Panel->Slot))
    {
        S->SetAnchors(FAnchors(0.f, 0.5f, 0.f, 0.5f));
        S->SetPosition(FVector2D(60.f, -PanelH * 0.5f));
        S->SetSize(FVector2D(PanelW, PanelH));
        S->SetAlignment(FVector2D::ZeroVector);
    }

    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
    Panel->AddChild(Scroll);

    Grid = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
    Grid->SetExplicitWrapSize(true);
    Grid->SetWrapSize(PanelW - 12.f);
    Scroll->AddChild(Grid);

    TooltipPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CraftTooltip"));
    TooltipPanel->SetBrush(MakeSolidBrush(FLinearColor(0.05f, 0.05f, 0.12f, 0.95f)));
    TooltipPanel->SetPadding(FMargin(8.f, 4.f));
    TooltipPanel->SetVisibility(ESlateVisibility::Hidden);
    Root->AddChild(TooltipPanel);

    TooltipText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    {
        FSlateFontInfo F = TooltipText->GetFont(); F.Size = 11; TooltipText->SetFont(F);
    }
    TooltipText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.92f, 0.75f)));
    TooltipPanel->AddChild(TooltipText);
}

// 蒐集「可用素材池」= 玩家 Inventory.Slots + （若有寶箱）寶箱 Inventory.Slots，依 ItemId 加總
static void AccumulatePool(const UInventoryComponent* Inv, TMap<EItemId, int32>& Pool)
{
    if (!Inv) return;
    for (const FItemStack& S : Inv->Slots)
        if (!S.IsEmpty())
            Pool.FindOrAdd(S.ItemId) += S.Count;
}

void UCraftingPanelWidget::RefreshCraftable(UInventoryComponent* InPlayerInv, UInventoryComponent* InChestInv, bool bHasWorkbench)
{
    PlayerInv = InPlayerInv;
    ChestInv  = InChestInv;
    bLastHasWorkbench = bHasWorkbench;

    TMap<EItemId, int32> Pool;
    AccumulatePool(InPlayerInv, Pool);
    AccumulatePool(InChestInv, Pool);

    TArray<EItemId> Craftable;
    for (const FRecipeEntry& Recipe : FRecipeRegistry::GetAll())
    {
        if (Recipe.bRequiresWorkbench && !bHasWorkbench) continue;
        bool bCanCraft = true;
        for (const FRecipeInput& In : Recipe.Inputs)
        {
            const int32* Have = Pool.Find(In.ItemId);
            if (!Have || *Have < In.Count) { bCanCraft = false; break; }
        }
        if (bCanCraft) Craftable.Add(Recipe.Output);
    }

    // 只在清單真的變動時才重建 widget tree（避免每幀 ClearChildren，
    // 同本次稽核發現的 RebuildList 同類問題教訓）
    if (Craftable != CachedCraftable)
    {
        CachedCraftable = Craftable;
        RebuildCards(Craftable);
    }
}

void UCraftingPanelWidget::RebuildCards(const TArray<EItemId>& Craftable)
{
    if (!Grid) return;
    Grid->ClearChildren();
    Cards.Reset();

    for (EItemId Id : Craftable)
    {
        UCraftingCardWidget* Card = CreateWidget<UCraftingCardWidget>(this);
        Card->Setup(Id, [this](EItemId Output) { OnCardClicked(Output); });
        if (UWrapBoxSlot* S = Grid->AddChildToWrapBox(Card))
            S->SetPadding(FMargin(2.f));
        Cards.Add(Card);
    }
}

void UCraftingPanelWidget::OnCardClicked(EItemId Output)
{
    const FRecipeEntry* Recipe = nullptr;
    for (const FRecipeEntry& R : FRecipeRegistry::GetAll())
        if (R.Output == Output) { Recipe = &R; break; }
    if (!Recipe) return;

    UInventoryComponent* P = PlayerInv.Get();
    if (!P) return;

    for (const FRecipeInput& In : Recipe->Inputs)
        ConsumeFromPools(In.ItemId, In.Count);

    P->TryAdd(Recipe->Output, Recipe->OutputCount);

    // 觸發即時重算（玩家手上消耗品數量已變，下一次 RefreshCraftable 自然會用新池子重算；
    // 這裡先用目前快取的 Inv 立即重算一次，避免畫面要等下一個 Tick 才更新）
    RefreshCraftable(PlayerInv.Get(), ChestInv.Get(), bLastHasWorkbench);
}

void UCraftingPanelWidget::ConsumeFromPools(EItemId Id, int32 Count)
{
    // 規格：「優先消耗玩家身上的素材」，數量不足的差額再從寶箱補
    UInventoryComponent* P = PlayerInv.Get();
    UInventoryComponent* C = ChestInv.Get();
    int32 Remaining = Count;

    if (P)
    {
        for (int32 i = 0; i < P->Slots.Num() && Remaining > 0; ++i)
        {
            if (P->Slots[i].ItemId != Id || P->Slots[i].IsEmpty()) continue;
            const int32 Take = FMath::Min(Remaining, P->Slots[i].Count);
            P->Consume(i, Take);
            Remaining -= Take;
        }
    }
    if (C)
    {
        for (int32 i = 0; i < C->Slots.Num() && Remaining > 0; ++i)
        {
            if (C->Slots[i].ItemId != Id || C->Slots[i].IsEmpty()) continue;
            const int32 Take = FMath::Min(Remaining, C->Slots[i].Count);
            C->Consume(i, Take);
            Remaining -= Take;
        }
    }
}

void UCraftingPanelWidget::NativeTick(const FGeometry& Geo, float Delta)
{
    Super::NativeTick(Geo, Delta);

    // Hover tooltip（名稱＋所需素材清單）
    for (const TObjectPtr<UCraftingCardWidget>& Card : Cards)
    {
        if (Card && Card->IsUnderMouse())
        {
            const EItemId Id = Card->GetItemId();
            FString Text = FItemRegistry::Get(Id).DisplayName.ToString();
            for (const FRecipeEntry& R : FRecipeRegistry::GetAll())
            {
                if (R.Output != Id) continue;
                Text += TEXT("\n需要：");
                for (const FRecipeInput& In : R.Inputs)
                    Text += FString::Printf(TEXT(" %s×%d"), *FItemRegistry::Get(In.ItemId).DisplayName.ToString(), In.Count);
                break;
            }
            if (TooltipText) TooltipText->SetText(FText::FromString(Text));
            if (TooltipPanel)
            {
                TooltipPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
                if (APlayerController* PC = GetOwningPlayer())
                {
                    float MX, MY; PC->GetMousePosition(MX, MY);
                    if (UCanvasPanelSlot* TS = Cast<UCanvasPanelSlot>(TooltipPanel->Slot))
                        TS->SetPosition(FVector2D(MX + 14.f, MY + 14.f));
                }
            }
            return;
        }
    }
    if (TooltipPanel) TooltipPanel->SetVisibility(ESlateVisibility::Hidden);
}
