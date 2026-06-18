#include "UGameFlowWidget.h"
#include "FlowSaveSystem.h"
#include "WorldSaveData.h"
#include "UGameSessionSubsystem.h"
#include "ASkillCreatorGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/ScrollBox.h"

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

// ── Widget 樹建構 ─────────────────────────────────────────────────────────

void UGameFlowWidget::BuildLayout()
{
    // 根節點需要明確、非零的 desired size，否則 VerticalBox 裡的 Fill-size
    // 子項（ScrollBox）無「可填滿的空間」可參照，整個選單可能變成不可見、不可
    // 點擊。原本用 CanvasPanel + 100% stretch anchors 包一層，懷疑是 CanvasPanel
    // 唯一子項為 stretch anchor 時其 desired size 算成 0x0、進而拖累整條鏈，
    // 但因背景測試視窗的 Slate 量測本身不可靠，未能 100% 釐清根因。改用 SizeBox
    // + 顯式 WidthOverride/HeightOverride：desired size 直接等於覆寫值，沒有
    // 任何依賴子項或 anchor 解析的模糊空間，是更穩妥的寫法。
    const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);

    USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>();
    RootSize->SetWidthOverride(ViewportSize.X);
    RootSize->SetHeightOverride(ViewportSize.Y);
    WidgetTree->RootWidget = RootSize;

    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.65f));
    RootSize->SetContent(Background);

    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
    Background->SetContent(Root);

    // 標題
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(FText::FromString(TEXT("─── 世界選擇 ───")));
    Root->AddChildToVerticalBox(Title);

    // 世界清單（可捲動）
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Scroll))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    WorldListContainer = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(WorldListContainer);

    // 建立世界列
    UHorizontalBox* CreateRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    Root->AddChildToVerticalBox(CreateRow);

    UTextBlock* NameLabel = WidgetTree->ConstructWidget<UTextBlock>();
    NameLabel->SetText(FText::FromString(TEXT("名稱：")));
    CreateRow->AddChildToHorizontalBox(NameLabel);

    NameInput = WidgetTree->ConstructWidget<UEditableText>();
    NameInput->SetHintText(FText::FromString(TEXT("輸入世界名稱")));
    CreateRow->AddChildToHorizontalBox(NameInput);

    UButton* CreateBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* CreateTxt = WidgetTree->ConstructWidget<UTextBlock>();
    CreateTxt->SetText(FText::FromString(TEXT("建立")));
    CreateBtn->AddChild(CreateTxt);
    CreateBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnCreateClicked);
    CreateRow->AddChildToHorizontalBox(CreateBtn);

    // 進入 / 刪除列（使用 World ID）
    UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    Root->AddChildToVerticalBox(ActionRow);

    UTextBlock* IdLabel = WidgetTree->ConstructWidget<UTextBlock>();
    IdLabel->SetText(FText::FromString(TEXT("World ID：")));
    ActionRow->AddChildToHorizontalBox(IdLabel);

    WorldIdInput = WidgetTree->ConstructWidget<UEditableText>();
    WorldIdInput->SetHintText(FText::FromString(TEXT("從清單複製 ID")));
    ActionRow->AddChildToHorizontalBox(WorldIdInput);

    UButton* EnterBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* EnterTxt = WidgetTree->ConstructWidget<UTextBlock>();
    EnterTxt->SetText(FText::FromString(TEXT("進入")));
    EnterBtn->AddChild(EnterTxt);
    EnterBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnEnterClicked);
    ActionRow->AddChildToHorizontalBox(EnterBtn);

    UButton* DeleteBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* DeleteTxt = WidgetTree->ConstructWidget<UTextBlock>();
    DeleteTxt->SetText(FText::FromString(TEXT("刪除")));
    DeleteBtn->AddChild(DeleteTxt);
    DeleteBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnDeleteClicked);
    ActionRow->AddChildToHorizontalBox(DeleteBtn);

    // 重新整理按鈕
    UButton* RefreshBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* RefreshTxt = WidgetTree->ConstructWidget<UTextBlock>();
    RefreshTxt->SetText(FText::FromString(TEXT("重新整理")));
    RefreshBtn->AddChild(RefreshTxt);
    RefreshBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnRefreshClicked);
    Root->AddChildToVerticalBox(RefreshBtn);

    // 狀態文字
    StatusText = WidgetTree->ConstructWidget<UTextBlock>();
    StatusText->SetText(FText::GetEmpty());
    Root->AddChildToVerticalBox(StatusText);
}

// ── UUserWidget overrides ─────────────────────────────────────────────────

void UGameFlowWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildLayout();
    RefreshWorldList();
}

// ── BlueprintNativeEvent 預設實作 ─────────────────────────────────────────

void UGameFlowWidget::OnWorldListRefreshed_Implementation(const TArray<FWorldSaveInfo>& Worlds)
{
    if (!WorldListContainer) return;
    WorldListContainer->ClearChildren();

    if (Worlds.IsEmpty())
    {
        UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>();
        Empty->SetText(FText::FromString(TEXT("（尚無世界，請建立）")));
        WorldListContainer->AddChildToVerticalBox(Empty);
        return;
    }

    for (const FWorldSaveInfo& W : Worlds)
    {
        UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>();
        Row->SetText(FText::FromString(
            FString::Printf(TEXT("● %s   (ID: %s)"), *W.Name, *W.Id)));
        WorldListContainer->AddChildToVerticalBox(Row);
    }
}

void UGameFlowWidget::OnEnterGame_Implementation(const FWorldSaveInfo& SelectedWorld)
{
    if (StatusText)
        StatusText->SetText(FText::FromString(
            FString::Printf(TEXT("進入世界：%s ..."), *SelectedWorld.Name)));

    // 單一地圖（MainMap）兼當選單與遊戲關卡，不需要 OpenLevel 換關卡；
    // EnterWorld() 已經把選好的 FWorldSaveData 寫進 UGameSessionSubsystem，這裡直接讀出來交給 GameMode。
    UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
    UGameSessionSubsystem* Sub = GI ? GI->GetSubsystem<UGameSessionSubsystem>() : nullptr;
    if (Sub && Sub->HasPendingWorld())
    {
        if (ASkillCreatorGameMode* GM = Cast<ASkillCreatorGameMode>(UGameplayStatics::GetGameMode(this)))
            GM->StartGameplayWithWorld(Sub->GetPendingWorld());
    }
}

// ── 按鈕回呼 ─────────────────────────────────────────────────────────────

void UGameFlowWidget::OnCreateClicked()
{
    if (!NameInput) return;
    const FString Name = NameInput->GetText().ToString().TrimStartAndEnd();
    if (Name.IsEmpty())
    {
        if (StatusText) StatusText->SetText(FText::FromString(TEXT("錯誤：請輸入世界名稱")));
        return;
    }
    const bool bOk = CreateWorld(Name, FMath::RandRange(1000, 99999));
    if (StatusText)
        StatusText->SetText(FText::FromString(
            bOk ? FString::Printf(TEXT("已建立：%s"), *Name) : TEXT("建立失敗")));
}

void UGameFlowWidget::OnEnterClicked()
{
    if (!WorldIdInput) return;
    const FString Id = WorldIdInput->GetText().ToString().TrimStartAndEnd();
    if (Id.IsEmpty())
    {
        if (StatusText) StatusText->SetText(FText::FromString(TEXT("錯誤：請輸入 World ID")));
        return;
    }
    EnterWorld(Id);
}

void UGameFlowWidget::OnDeleteClicked()
{
    if (!WorldIdInput) return;
    const FString Id = WorldIdInput->GetText().ToString().TrimStartAndEnd();
    if (Id.IsEmpty())
    {
        if (StatusText) StatusText->SetText(FText::FromString(TEXT("錯誤：請輸入 World ID")));
        return;
    }
    const bool bOk = RemoveWorld(Id);
    if (StatusText)
        StatusText->SetText(FText::FromString(
            bOk ? FString::Printf(TEXT("已刪除 ID: %s"), *Id) : TEXT("刪除失敗（ID 不存在？）")));
}

void UGameFlowWidget::OnRefreshClicked()
{
    RefreshWorldList();
    if (StatusText) StatusText->SetText(FText::FromString(TEXT("清單已更新")));
}

// ── BlueprintCallable ─────────────────────────────────────────────────────

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

    if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
        if (UGameSessionSubsystem* Sub = GI->GetSubsystem<UGameSessionSubsystem>())
            Sub->SetPendingWorld(Meta);

    OnEnterGame(*Found);
}
