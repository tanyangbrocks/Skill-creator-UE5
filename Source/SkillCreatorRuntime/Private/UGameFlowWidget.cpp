#include "UGameFlowWidget.h"
#include "FlowSaveSystem.h"
#include "WorldSaveData.h"
#include "UGameSessionSubsystem.h"
#include "ASkillCreatorGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
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

static FCharacterSummaryInfo ToSummary(const FCharacterSaveData& D)
{
    FCharacterSummaryInfo I;
    I.Id    = D.Id;
    I.Name  = D.CharacterName;
    I.Level = D.Level;
    return I;
}

// ── Widget 樹建構 ─────────────────────────────────────────────────────────

void UGameFlowWidget::BuildLayout()
{
    // 根節點直接用 UBorder（不再用 SizeBox + GetViewportSize）。
    // GameMode 呼叫端已設 SetAnchorsInViewport(0,0,1,1) + SetOffsetsInViewport(0,0,0,0)，
    // viewport slot 的大小等於整個螢幕，UBorder 填滿 slot 即可，不需要再問 viewport 尺寸。
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.65f));
    WidgetTree->RootWidget = Background;

    // 角色畫面 / 世界畫面疊在同一位置，靠 ShowScreen() 切換可見性
    // （對應 Godot ShowScreen：同一時間只顯示一個 Panel）
    UOverlay* ScreenStack = WidgetTree->ConstructWidget<UOverlay>();
    Background->SetContent(ScreenStack);

    CharScreen  = BuildCharScreen();
    WorldScreen = BuildWorldScreen();

    // UOverlaySlot 預設是 HAlign_Left/VAlign_Top（只給內容本身的 desired size），
    // 不是 Fill —— 沒有明確設成 Fill 的話，兩個畫面又會退回跟 Border/CanvasPanel
    // 同一類「子項拿不到完整可用空間」的問題。
    if (UOverlaySlot* CharSlot = ScreenStack->AddChildToOverlay(CharScreen))
    {
        CharSlot->SetHorizontalAlignment(HAlign_Fill);
        CharSlot->SetVerticalAlignment(VAlign_Fill);
    }
    if (UOverlaySlot* WorldSlot = ScreenStack->AddChildToOverlay(WorldScreen))
    {
        WorldSlot->SetHorizontalAlignment(HAlign_Fill);
        WorldSlot->SetVerticalAlignment(VAlign_Fill);
    }

    ShowScreen(CharScreen);

    // ── 確認彈窗（疊在最上層，預設隱藏）────────────────────────────
    UBorder* ConfirmBg = WidgetTree->ConstructWidget<UBorder>();
    ConfirmBg->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.08f, 0.95f));

    UVerticalBox* ConfirmBox = WidgetTree->ConstructWidget<UVerticalBox>();
    ConfirmBg->SetContent(ConfirmBox);

    ConfirmMsgText = WidgetTree->ConstructWidget<UTextBlock>();
    ConfirmMsgText->SetText(FText::FromString(TEXT("確定要刪除？")));
    ConfirmBox->AddChildToVerticalBox(ConfirmMsgText);

    UHorizontalBox* ConfirmBtns = WidgetTree->ConstructWidget<UHorizontalBox>();
    ConfirmBox->AddChildToVerticalBox(ConfirmBtns);

    UButton* YesBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* YesTxt = WidgetTree->ConstructWidget<UTextBlock>();
    YesTxt->SetText(FText::FromString(TEXT("確認刪除")));
    YesBtn->AddChild(YesTxt);
    YesBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnConfirmDeleteYes);
    ConfirmBtns->AddChildToHorizontalBox(YesBtn);

    UButton* NoBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* NoTxt = WidgetTree->ConstructWidget<UTextBlock>();
    NoTxt->SetText(FText::FromString(TEXT("取消")));
    NoBtn->AddChild(NoTxt);
    NoBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnConfirmDeleteNo);
    ConfirmBtns->AddChildToHorizontalBox(NoBtn);

    ConfirmOverlay = ConfirmBg;
    if (UOverlaySlot* ConfirmSlot = ScreenStack->AddChildToOverlay(ConfirmOverlay))
    {
        ConfirmSlot->SetHorizontalAlignment(HAlign_Center);
        ConfirmSlot->SetVerticalAlignment(VAlign_Center);
    }
    ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
}

UWidget* UGameFlowWidget::BuildCharScreen()
{
    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();

    // 標題
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(FText::FromString(TEXT("─── 選擇角色 ───")));
    Root->AddChildToVerticalBox(Title);

    // 角色清單（可捲動）
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Scroll))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    CharListContainer = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(CharListContainer);

    // 建立角色列
    UHorizontalBox* CreateRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    Root->AddChildToVerticalBox(CreateRow);

    UTextBlock* NameLabel = WidgetTree->ConstructWidget<UTextBlock>();
    NameLabel->SetText(FText::FromString(TEXT("名稱：")));
    CreateRow->AddChildToHorizontalBox(NameLabel);

    CharNameInput = WidgetTree->ConstructWidget<UEditableText>();
    CharNameInput->SetHintText(FText::FromString(TEXT("輸入角色名稱")));
    CreateRow->AddChildToHorizontalBox(CharNameInput);

    UButton* CreateBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* CreateTxt = WidgetTree->ConstructWidget<UTextBlock>();
    CreateTxt->SetText(FText::FromString(TEXT("建立")));
    CreateBtn->AddChild(CreateTxt);
    CreateBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnCharCreateClicked);
    CreateRow->AddChildToHorizontalBox(CreateBtn);

    // 選擇 / 刪除列（使用 Character ID）
    UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    Root->AddChildToVerticalBox(ActionRow);

    UTextBlock* IdLabel = WidgetTree->ConstructWidget<UTextBlock>();
    IdLabel->SetText(FText::FromString(TEXT("Character ID：")));
    ActionRow->AddChildToHorizontalBox(IdLabel);

    CharIdInput = WidgetTree->ConstructWidget<UEditableText>();
    CharIdInput->SetHintText(FText::FromString(TEXT("從清單複製 ID")));
    ActionRow->AddChildToHorizontalBox(CharIdInput);

    UButton* SelectBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* SelectTxt = WidgetTree->ConstructWidget<UTextBlock>();
    SelectTxt->SetText(FText::FromString(TEXT("選擇")));
    SelectBtn->AddChild(SelectTxt);
    SelectBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnCharSelectClicked);
    ActionRow->AddChildToHorizontalBox(SelectBtn);

    UButton* DeleteBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* DeleteTxt = WidgetTree->ConstructWidget<UTextBlock>();
    DeleteTxt->SetText(FText::FromString(TEXT("刪除")));
    DeleteBtn->AddChild(DeleteTxt);
    DeleteBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnCharDeleteClicked);
    ActionRow->AddChildToHorizontalBox(DeleteBtn);

    // 重新整理按鈕
    UButton* RefreshBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* RefreshTxt = WidgetTree->ConstructWidget<UTextBlock>();
    RefreshTxt->SetText(FText::FromString(TEXT("重新整理")));
    RefreshBtn->AddChild(RefreshTxt);
    RefreshBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnCharRefreshClicked);
    Root->AddChildToVerticalBox(RefreshBtn);

    // 狀態文字（角色/世界畫面共用同一個 StatusText 實例）
    StatusText = WidgetTree->ConstructWidget<UTextBlock>();
    StatusText->SetText(FText::GetEmpty());
    Root->AddChildToVerticalBox(StatusText);

    return Root;
}

UWidget* UGameFlowWidget::BuildWorldScreen()
{
    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();

    // 標題
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(FText::FromString(TEXT("─── 選擇世界 ───")));
    Root->AddChildToVerticalBox(Title);

    // 返回按鈕（回到角色畫面，重新選角色）
    UButton* BackBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* BackTxt = WidgetTree->ConstructWidget<UTextBlock>();
    BackTxt->SetText(FText::FromString(TEXT("← 返回角色選擇")));
    BackBtn->AddChild(BackTxt);
    BackBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnWorldBackClicked);
    Root->AddChildToVerticalBox(BackBtn);

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

    WorldNameInput = WidgetTree->ConstructWidget<UEditableText>();
    WorldNameInput->SetHintText(FText::FromString(TEXT("輸入世界名稱")));
    CreateRow->AddChildToHorizontalBox(WorldNameInput);

    UButton* CreateBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* CreateTxt = WidgetTree->ConstructWidget<UTextBlock>();
    CreateTxt->SetText(FText::FromString(TEXT("建立")));
    CreateBtn->AddChild(CreateTxt);
    CreateBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnWorldCreateClicked);
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
    EnterBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnWorldEnterClicked);
    ActionRow->AddChildToHorizontalBox(EnterBtn);

    UButton* DeleteBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* DeleteTxt = WidgetTree->ConstructWidget<UTextBlock>();
    DeleteTxt->SetText(FText::FromString(TEXT("刪除")));
    DeleteBtn->AddChild(DeleteTxt);
    DeleteBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnWorldDeleteClicked);
    ActionRow->AddChildToHorizontalBox(DeleteBtn);

    // 重新整理按鈕
    UButton* RefreshBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* RefreshTxt = WidgetTree->ConstructWidget<UTextBlock>();
    RefreshTxt->SetText(FText::FromString(TEXT("重新整理")));
    RefreshBtn->AddChild(RefreshTxt);
    RefreshBtn->OnClicked.AddDynamic(this, &UGameFlowWidget::OnWorldRefreshClicked);
    Root->AddChildToVerticalBox(RefreshBtn);

    return Root;
}

void UGameFlowWidget::ShowScreen(UWidget* Target)
{
    if (CharScreen)  CharScreen->SetVisibility(Target == CharScreen  ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (WorldScreen) WorldScreen->SetVisibility(Target == WorldScreen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

// ── UUserWidget overrides ─────────────────────────────────────────────────

void UGameFlowWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildLayout();
    RefreshCharacterList();
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

void UGameFlowWidget::OnCharacterListRefreshed_Implementation(const TArray<FCharacterSummaryInfo>& Characters)
{
    if (!CharListContainer) return;
    CharListContainer->ClearChildren();

    if (Characters.IsEmpty())
    {
        UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>();
        Empty->SetText(FText::FromString(TEXT("（尚無角色，請建立）")));
        CharListContainer->AddChildToVerticalBox(Empty);
        return;
    }

    for (const FCharacterSummaryInfo& C : Characters)
    {
        UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>();
        Row->SetText(FText::FromString(
            FString::Printf(TEXT("● %s   LV %d   (ID: %s)"), *C.Name, C.Level, *C.Id)));
        CharListContainer->AddChildToVerticalBox(Row);
    }
}

void UGameFlowWidget::OnEnterGame_Implementation(const FWorldSaveInfo& SelectedWorld)
{
    if (StatusText)
        StatusText->SetText(FText::FromString(
            FString::Printf(TEXT("進入世界：%s ..."), *SelectedWorld.Name)));

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

// ── 按鈕回呼（角色畫面）──────────────────────────────────────────────────

void UGameFlowWidget::OnCharCreateClicked()
{
    if (!CharNameInput) return;
    const FString Name = CharNameInput->GetText().ToString().TrimStartAndEnd();
    if (Name.IsEmpty())
    {
        if (StatusText) StatusText->SetText(FText::FromString(TEXT("錯誤：請輸入角色名稱")));
        return;
    }
    const bool bOk = CreateCharacter(Name);
    if (StatusText)
        StatusText->SetText(FText::FromString(
            bOk ? FString::Printf(TEXT("已建立：%s"), *Name) : TEXT("建立失敗")));
}

void UGameFlowWidget::OnCharSelectClicked()
{
    if (!CharIdInput) return;
    const FString Id = CharIdInput->GetText().ToString().TrimStartAndEnd();
    if (Id.IsEmpty())
    {
        if (StatusText) StatusText->SetText(FText::FromString(TEXT("錯誤：請輸入 Character ID")));
        return;
    }
    SelectCharacter(Id);
}

void UGameFlowWidget::OnCharDeleteClicked()
{
    if (!CharIdInput) return;
    const FString Id = CharIdInput->GetText().ToString().TrimStartAndEnd();
    if (Id.IsEmpty())
    {
        if (StatusText) StatusText->SetText(FText::FromString(TEXT("錯誤：請輸入 Character ID")));
        return;
    }
    ShowConfirmDelete(Id, /*bIsChar=*/true);
}

void UGameFlowWidget::OnCharRefreshClicked()
{
    RefreshCharacterList();
    if (StatusText) StatusText->SetText(FText::FromString(TEXT("清單已更新")));
}

// ── 按鈕回呼（世界畫面）──────────────────────────────────────────────────

void UGameFlowWidget::OnWorldCreateClicked()
{
    if (!WorldNameInput) return;
    const FString Name = WorldNameInput->GetText().ToString().TrimStartAndEnd();
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

void UGameFlowWidget::OnWorldEnterClicked()
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

void UGameFlowWidget::OnWorldDeleteClicked()
{
    if (!WorldIdInput) return;
    const FString Id = WorldIdInput->GetText().ToString().TrimStartAndEnd();
    if (Id.IsEmpty())
    {
        if (StatusText) StatusText->SetText(FText::FromString(TEXT("錯誤：請輸入 World ID")));
        return;
    }
    ShowConfirmDelete(Id, /*bIsChar=*/false);
}

void UGameFlowWidget::ShowConfirmDelete(const FString& Id, bool bIsChar)
{
    PendingDeleteId      = Id;
    bPendingDeleteIsChar = bIsChar;
    if (ConfirmMsgText)
    {
        FString TypeName = bIsChar ? TEXT("角色") : TEXT("世界");
        ConfirmMsgText->SetText(FText::FromString(
            FString::Printf(TEXT("確定要刪除%s 「%s」？此操作不可逆。"), *TypeName, *Id)));
    }
    if (ConfirmOverlay)
        ConfirmOverlay->SetVisibility(ESlateVisibility::Visible);
}

void UGameFlowWidget::OnConfirmDeleteYes()
{
    if (ConfirmOverlay)
        ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);

    bool bOk = false;
    if (bPendingDeleteIsChar)
        bOk = RemoveCharacter(PendingDeleteId);
    else
        bOk = RemoveWorld(PendingDeleteId);

    if (StatusText)
        StatusText->SetText(FText::FromString(
            bOk ? FString::Printf(TEXT("已刪除 ID: %s"), *PendingDeleteId)
                : TEXT("刪除失敗（ID 不存在？）")));
    PendingDeleteId.Empty();
}

void UGameFlowWidget::OnConfirmDeleteNo()
{
    if (ConfirmOverlay)
        ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
    PendingDeleteId.Empty();
}

void UGameFlowWidget::OnWorldRefreshClicked()
{
    RefreshWorldList();
    if (StatusText) StatusText->SetText(FText::FromString(TEXT("清單已更新")));
}

void UGameFlowWidget::OnWorldBackClicked()
{
    ShowScreen(CharScreen);
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
    if (!FFlowSaveSystem::LoadCharacter(CharacterId, Loaded))
    {
        if (StatusText) StatusText->SetText(FText::FromString(TEXT("錯誤：找不到該角色 ID")));
        return;
    }

    SelectedCharacter = Loaded;

    if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
        if (UGameSessionSubsystem* Sub = GI->GetSubsystem<UGameSessionSubsystem>())
            Sub->SetPendingCharacter(SelectedCharacter);

    RefreshWorldList();
    ShowScreen(WorldScreen);
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
