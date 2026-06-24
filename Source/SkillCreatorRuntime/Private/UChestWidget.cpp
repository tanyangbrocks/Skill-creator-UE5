#include "UChestWidget.h"
#include "UInventoryWidget.h"
#include "UInventoryComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

void UChestWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // 標題（對應規格「玩家打開寶箱，玩家自己的物品欄也會自動打開」，兩欄並排顯示）
    UTextBlock* TitleLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TitleLbl->SetText(FText::FromString(TEXT("木寶箱  [右鍵再按一次關閉]")));
    {
        FSlateFontInfo F = TitleLbl->GetFont(); F.Size = 13; TitleLbl->SetFont(F);
    }
    TitleLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.80f, 0.55f)));
    Root->AddChild(TitleLbl);
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(TitleLbl->Slot))
    {
        S->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f));
        S->SetPosition(FVector2D(-150.f, 4.f));
        S->SetSize(FVector2D(300.f, 18.f));
        S->SetAlignment(FVector2D::ZeroVector);
    }

    // 兩個獨立 UInventoryWidget 實例並排（左：寶箱，右：玩家背包——跟既有 UInventoryWidget
    // 自身的版面寬度配合，各自佔一個夠大的 Canvas 區塊，內部錨點相對自己的格子計算）
    constexpr float SlotW = 360.f, SlotH = 420.f, Gap = 12.f;

    ChestInvWidget = CreateWidget<UInventoryWidget>(this);
    Root->AddChild(ChestInvWidget);
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(ChestInvWidget->Slot))
    {
        S->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f));
        S->SetPosition(FVector2D(-SlotW - Gap * 0.5f, 26.f));
        S->SetSize(FVector2D(SlotW, SlotH));
        S->SetAlignment(FVector2D::ZeroVector);
    }

    PlayerInvWidget = CreateWidget<UInventoryWidget>(this);
    Root->AddChild(PlayerInvWidget);
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(PlayerInvWidget->Slot))
    {
        S->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f));
        S->SetPosition(FVector2D(Gap * 0.5f, 26.f));
        S->SetSize(FVector2D(SlotW, SlotH));
        S->SetAlignment(FVector2D::ZeroVector);
    }

    // 跨欄拖曳互相配對
    PlayerInvWidget->SetPairedWidget(ChestInvWidget);
    ChestInvWidget->SetPairedWidget(PlayerInvWidget);

    PlayerInvWidget->OnSlotSwapRequested.BindLambda([this](int32 A, int32 B)
    {
        if (UInventoryComponent* Inv = PlayerInv.Get()) Inv->SwapSlots(A, B);
        RefreshBoth();
    });
    ChestInvWidget->OnSlotSwapRequested.BindLambda([this](int32 A, int32 B)
    {
        if (UInventoryComponent* Inv = ChestInv.Get()) Inv->SwapSlots(A, B);
        RefreshBoth();
    });
    // 跨欄：直接交換兩個不同 UInventoryComponent 的 Slots 陣列元素（語意同 SwapSlots，
    // 只是來源/目的地分屬兩個元件，Slots 是公開 UPROPERTY 陣列可直接存取）
    PlayerInvWidget->OnCrossSwapRequested.BindLambda([this](int32 SrcSlot, int32 DstSlot)
    {
        if (UInventoryComponent* P = PlayerInv.Get())
        if (UInventoryComponent* C = ChestInv.Get())
        if (P->Slots.IsValidIndex(SrcSlot) && C->Slots.IsValidIndex(DstSlot))
            Swap(P->Slots[SrcSlot], C->Slots[DstSlot]);
        RefreshBoth();
    });
    ChestInvWidget->OnCrossSwapRequested.BindLambda([this](int32 SrcSlot, int32 DstSlot)
    {
        if (UInventoryComponent* C = ChestInv.Get())
        if (UInventoryComponent* P = PlayerInv.Get())
        if (C->Slots.IsValidIndex(SrcSlot) && P->Slots.IsValidIndex(DstSlot))
            Swap(C->Slots[SrcSlot], P->Slots[DstSlot]);
        RefreshBoth();
    });
}

void UChestWidget::Setup(UInventoryComponent* InPlayerInv, UInventoryComponent* InChestInv)
{
    PlayerInv = InPlayerInv;
    ChestInv  = InChestInv;
    RefreshBoth();
}

void UChestWidget::RefreshBoth()
{
    if (PlayerInvWidget && PlayerInv.IsValid()) PlayerInvWidget->Refresh(PlayerInv.Get());
    if (ChestInvWidget  && ChestInv.IsValid())  ChestInvWidget->Refresh(ChestInv.Get());
}
