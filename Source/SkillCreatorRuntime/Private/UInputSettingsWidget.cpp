#include "UInputSettingsWidget.h"
#include "ASkillCreatorCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Framework/Application/SlateApplication.h"

const TCHAR* UInputSettingsWidget::ConfigSection = TEXT("PlayerInputBindings");

// ── UKeyRebindProxy ───────────────────────────────────────────────────────────

void UKeyRebindProxy::OnClick()
{
    if (Owner.IsValid() && Border.IsValid())
        Owner->BeginRebind(RebindId, ActionName, OrigKey, Border.Get());
}

// ── 顯示名稱對應表 ─────────────────────────────────────────────────────────────

FString UInputSettingsWidget::GetDisplayName(const FString& ActionName, const FKey& Key)
{
    // IA_Move 依照預設鍵位決定方向（預設 W/S/A/D 不變時成立）
    if (ActionName == TEXT("IA_Move"))
    {
        if (Key == EKeys::W) return TEXT("向前移動");
        if (Key == EKeys::S) return TEXT("向後移動");
        if (Key == EKeys::A) return TEXT("向左移動");
        if (Key == EKeys::D) return TEXT("向右移動");
        return TEXT("移動");
    }
    static const TMap<FString, FString> kNames = {
        { TEXT("IA_Look"),        TEXT("視角（滑鼠）") },
        { TEXT("Jump"),           TEXT("跳躍") },
        { TEXT("Mine"),           TEXT("採掘") },
        { TEXT("PrimaryTap"),     TEXT("主動互動") },
        { TEXT("Place"),          TEXT("放置") },
        { TEXT("SpellU"),         TEXT("施法 U") },
        { TEXT("SpellI"),         TEXT("施法 I") },
        { TEXT("SpellO"),         TEXT("施法 O") },
        { TEXT("SpellP"),         TEXT("施法 P") },
        { TEXT("ToggleCamera"),   TEXT("切換視角") },
        { TEXT("DebugTrace"),     TEXT("[偵錯] 追蹤") },
        { TEXT("SnapshotTake"),   TEXT("[偵錯] 快照") },
        { TEXT("SnapshotApply"),  TEXT("[偵錯] 套用快照") },
        { TEXT("DbgPaint"),       TEXT("[偵錯] 筆刷") },
        { TEXT("DbgCoord"),       TEXT("[偵錯] 座標") },
        { TEXT("DbgSurvival"),    TEXT("[偵錯] 生存資訊") },
    };
    if (const FString* V = kNames.Find(ActionName)) return *V;
    return ActionName;
}

bool UInputSettingsWidget::IsRemappable(const FString& ActionName, const FKey& Key)
{
    // 滑鼠軸向無意義重綁
    if (Key == EKeys::MouseX || Key == EKeys::MouseY) return false;
    // 視角動作非重綁對象
    if (ActionName == TEXT("IA_Look")) return false;
    return true;
}

// ── 工具 ─────────────────────────────────────────────────────────────────────

FString UInputSettingsWidget::MakeRebindId(const FString& ActionName, const FKey& Key)
{
    return ActionName + TEXT("|") + Key.ToString();
}

FString UInputSettingsWidget::KeysToConfigStr(const TArray<FKey>& Keys)
{
    TArray<FString> Parts;
    for (const FKey& K : Keys) Parts.Add(K.ToString());
    return FString::Join(Parts, TEXT(","));
}

TArray<FKey> UInputSettingsWidget::ConfigStrToKeys(const FString& S)
{
    TArray<FKey> Out;
    TArray<FString> Parts;
    S.ParseIntoArray(Parts, TEXT(","), true);
    for (const FString& P : Parts)
    {
        FKey K(FName(*P.TrimStartAndEnd()));
        if (K.IsValid()) Out.Add(K);
    }
    return Out;
}

FString UInputSettingsWidget::ConfigFilePath()
{
    return FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("PlayerInputBindings.ini");
}

UEnhancedInputLocalPlayerSubsystem* UInputSettingsWidget::GetInputSub() const
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC) return nullptr;
    ULocalPlayer* LP = PC->GetLocalPlayer();
    if (!LP) return nullptr;
    return LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
}

UInputMappingContext* UInputSettingsWidget::GetDefaultIMC() const
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC) return nullptr;
    ASkillCreatorCharacter* Char = Cast<ASkillCreatorCharacter>(PC->GetPawn());
    if (!Char) return nullptr;
    return Char->GetDefaultMappingContext();
}

UInputAction* UInputSettingsWidget::FindActionByName(const FString& ActionName) const
{
    UInputMappingContext* IMC = GetDefaultIMC();
    if (!IMC) return nullptr;
    for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
        if (M.Action && M.Action->GetName() == ActionName)
            return const_cast<UInputAction*>(M.Action.Get());
    return nullptr;
}

void UInputSettingsWidget::RefreshSubsystem() const
{
    UInputMappingContext* IMC = GetDefaultIMC();
    UEnhancedInputLocalPlayerSubsystem* Sub = GetInputSub();
    if (!IMC || !Sub) return;
    Sub->RemoveMappingContext(IMC);
    Sub->AddMappingContext(IMC, 0);
}

bool UInputSettingsWidget::RemapActionInIMC(const FString& ActionName,
                                            const FKey& OldKey, const FKey& NewKey)
{
    UInputMappingContext* IMC = GetDefaultIMC();
    if (!IMC) return false;
    UInputAction* Action = FindActionByName(ActionName);
    if (!Action) return false;
    IMC->UnmapKey(Action, OldKey);
    IMC->MapKey(Action, NewKey);
    RefreshSubsystem();
    return true;
}

// ── GetCurrentBindings ────────────────────────────────────────────────────────

TArray<FKeyBindingEntry> UInputSettingsWidget::GetCurrentBindings() const
{
    TArray<FKeyBindingEntry> Result;
    UInputMappingContext* IMC = GetDefaultIMC();
    if (!IMC) return Result;

    const FString CfgPath = ConfigFilePath();
    const bool bHasCfg = FPaths::FileExists(CfgPath);

    for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
    {
        if (!M.Action) continue;
        FKeyBindingEntry Entry;
        Entry.ActionName = M.Action->GetName();
        Entry.bIsRemappable = IsRemappable(Entry.ActionName, M.Key);

        // 主鍵來自 IMC；Config 可能記錄了完整組合鍵清單
        if (bHasCfg)
        {
            FString ComboStr;
            const FString CfgKey = MakeRebindId(Entry.ActionName, M.Key);
            // 先用 RebindId 找（對應「重綁後儲存」的 key）
            if (!GConfig->GetString(ConfigSection, *CfgKey, ComboStr, CfgPath))
                // 相容舊格式（只存 ActionName）
                GConfig->GetString(ConfigSection, *Entry.ActionName, ComboStr, CfgPath);

            TArray<FKey> Loaded = ConfigStrToKeys(ComboStr);
            if (Loaded.Num() > 0)
                Entry.BoundKeys = Loaded;
        }

        // 若 Config 未提供，BoundKeys 只放 IMC 主鍵
        if (Entry.BoundKeys.IsEmpty())
            Entry.BoundKeys.Add(M.Key);

        // 確保 BoundKeys[0] 與 IMC 主鍵一致（Config 值可能過時）
        if (Entry.BoundKeys[0] != M.Key)
        {
            Entry.BoundKeys.Empty();
            Entry.BoundKeys.Add(M.Key);
        }

        if (const FKey* Orig = OriginalKeys.Find(FName(*Entry.ActionName)))
            Entry.bIsCustomized = (M.Key != *Orig);

        Result.Add(Entry);
    }
    return Result;
}

// ── Widget 建構 ───────────────────────────────────────────────────────────────

void UInputSettingsWidget::BuildLayout()
{
    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
    WidgetTree->RootWidget = Root;

    // 捲動鍵位清單
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Scroll))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    BindingListContainer = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(BindingListContainer);

    // 操作按鈕列
    UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    Root->AddChildToVerticalBox(BtnRow);

    // AddDynamic 是巨集，第二個參數不能傳 lambda 參數（字串化結果不是 ClassName::Method）
    {
        UButton* Btn = WidgetTree->ConstructWidget<UButton>();
        UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
        Txt->SetText(FText::FromString(TEXT("儲存")));
        Txt->SetColorAndOpacity(FSlateColor(FLinearColor(0.1f, 0.1f, 0.2f)));
        Btn->AddChild(Txt);
        Btn->OnClicked.AddDynamic(this, &UInputSettingsWidget::OnSaveClicked);
        if (UHorizontalBoxSlot* HS = Cast<UHorizontalBoxSlot>(BtnRow->AddChild(Btn)))
            HS->SetPadding(FMargin(4.f, 8.f));
    }
    {
        UButton* Btn = WidgetTree->ConstructWidget<UButton>();
        UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>();
        Txt->SetText(FText::FromString(TEXT("恢復預設")));
        Txt->SetColorAndOpacity(FSlateColor(FLinearColor(0.1f, 0.1f, 0.2f)));
        Btn->AddChild(Txt);
        Btn->OnClicked.AddDynamic(this, &UInputSettingsWidget::OnResetClicked);
        if (UHorizontalBoxSlot* HS = Cast<UHorizontalBoxSlot>(BtnRow->AddChild(Btn)))
            HS->SetPadding(FMargin(4.f, 8.f));
    }

    StatusText = WidgetTree->ConstructWidget<UTextBlock>();
    StatusText->SetText(FText::GetEmpty());
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.6f, 0.2f)));
    Root->AddChildToVerticalBox(StatusText);
}

void UInputSettingsWidget::RebuildRows(const TArray<FKeyBindingEntry>& Entries)
{
    if (!BindingListContainer) return;
    BindingListContainer->ClearChildren();
    ActionBorders.Empty();
    RowProxies.Empty();

    const FLinearColor kRowBg    = FLinearColor(0.94f, 0.96f, 0.99f);
    const FLinearColor kRowBgAlt = FLinearColor(0.89f, 0.92f, 0.97f);
    const FLinearColor kKeyBg    = FLinearColor(0.80f, 0.85f, 0.95f);
    const FLinearColor kText     = FLinearColor(0.1f,  0.1f,  0.2f);
    const FLinearColor kGray     = FLinearColor(0.5f,  0.5f,  0.5f);

    for (int32 i = 0; i < Entries.Num(); ++i)
    {
        const FKeyBindingEntry& E = Entries[i];

        // 行 border
        UBorder* RowBorder = WidgetTree->ConstructWidget<UBorder>();
        FSlateBrush RowBrush;
        RowBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        RowBrush.TintColor = FSlateColor(i % 2 == 0 ? kRowBg : kRowBgAlt);
        RowBorder->SetBrush(RowBrush);
        if (UVerticalBoxSlot* VS = Cast<UVerticalBoxSlot>(
                BindingListContainer->AddChild(RowBorder)))
            VS->SetPadding(FMargin(0.f, 1.f));

        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        if (UBorderSlot* BS = Cast<UBorderSlot>(RowBorder->AddChild(Row)))
            BS->SetPadding(FMargin(8.f, 6.f));

        // 左：動作名稱
        USizeBox* NameBox = WidgetTree->ConstructWidget<USizeBox>();
        NameBox->SetWidthOverride(200.f);
        UTextBlock* NameTxt = WidgetTree->ConstructWidget<UTextBlock>();
        const FString DisplayStr = GetDisplayName(E.ActionName, E.BoundKeys.IsEmpty() ? EKeys::Invalid : E.BoundKeys[0]);
        NameTxt->SetText(FText::FromString(
            E.bIsCustomized ? FString(TEXT("* ")) + DisplayStr : DisplayStr));
        NameTxt->SetColorAndOpacity(FSlateColor(kText));
        NameBox->AddChild(NameTxt);
        Row->AddChildToHorizontalBox(NameBox);

        // 右：鍵位顯示格（可點擊→重綁）
        UBorder* KeyBorder = WidgetTree->ConstructWidget<UBorder>();
        FSlateBrush KeyBrush;
        KeyBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        KeyBrush.TintColor = FSlateColor(kKeyBg);
        KeyBorder->SetBrush(KeyBrush);

        // 顯示組合鍵文字（如 "W" 或 "U + LeftCtrl"）
        FString KeyStr;
        for (int32 k = 0; k < E.BoundKeys.Num(); ++k)
        {
            if (k > 0) KeyStr += TEXT(" + ");
            KeyStr += E.BoundKeys[k].ToString();
        }
        UTextBlock* KeyTxt = WidgetTree->ConstructWidget<UTextBlock>();
        KeyTxt->SetText(FText::FromString(KeyStr));
        KeyTxt->SetColorAndOpacity(FSlateColor(E.bIsRemappable ? kText : kGray));
        if (UBorderSlot* KBS = Cast<UBorderSlot>(KeyBorder->AddChild(KeyTxt)))
            KBS->SetPadding(FMargin(10.f, 4.f));

        if (E.bIsRemappable)
        {
            // 可重綁：用 UButton 包住 KeyBorder
            UButton* KeyBtn = WidgetTree->ConstructWidget<UButton>();
            FSlateBrush BtnBrush;
            BtnBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
            FButtonStyle BtnStyle;
            BtnStyle.Normal = BtnStyle.Hovered = BtnStyle.Pressed = BtnBrush;
            KeyBtn->SetStyle(BtnStyle);
            KeyBtn->AddChild(KeyBorder);
            if (UHorizontalBoxSlot* HS = Cast<UHorizontalBoxSlot>(
                    Row->AddChildToHorizontalBox(KeyBtn)))
                HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

            // Dynamic delegate 不支援 AddLambda，使用 Proxy UObject 轉發
            const FString RebindId = MakeRebindId(E.ActionName,
                E.BoundKeys.IsEmpty() ? EKeys::Invalid : E.BoundKeys[0]);
            UKeyRebindProxy* Proxy = NewObject<UKeyRebindProxy>(this);
            Proxy->RebindId   = RebindId;
            Proxy->ActionName = E.ActionName;
            Proxy->OrigKey    = E.BoundKeys.IsEmpty() ? EKeys::Invalid : E.BoundKeys[0];
            Proxy->Owner      = this;
            Proxy->Border     = KeyBorder;
            RowProxies.Add(Proxy);
            KeyBtn->OnClicked.AddDynamic(Proxy, &UKeyRebindProxy::OnClick);

            ActionBorders.Add(RebindId, KeyBorder);
        }
        else
        {
            if (UHorizontalBoxSlot* HS = Cast<UHorizontalBoxSlot>(
                    Row->AddChildToHorizontalBox(KeyBorder)))
                HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }
    }
}

// ── 初始化 ────────────────────────────────────────────────────────────────────

void UInputSettingsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();

    // 原始鍵位快照（Load 前，每個 action 取第一個 mapping 的 key）
    OriginalKeys.Empty();
    if (UInputMappingContext* IMC = GetDefaultIMC())
        for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
            if (M.Action && !OriginalKeys.Contains(FName(*M.Action->GetName())))
                OriginalKeys.Add(FName(*M.Action->GetName()), M.Key);

    LoadAndApplyBindings();
    RebuildRows(GetCurrentBindings());
}

// ── 重綁狀態機 ───────────────────────────────────────────────────────────────

void UInputSettingsWidget::BeginRebind(const FString& RebindId, const FString& ActionName,
                                       const FKey& OrigKey, UBorder* RowBorder)
{
    // 若已在等待中，先取消舊的
    if (bAwaitingInput) CancelRebind();

    PendingRebindId   = RebindId;
    PendingActionName = ActionName;
    PendingOrigKey    = OrigKey;
    PendingBorder     = RowBorder;
    PendingKeys.Empty();
    KeysCurrentlyHeld.Empty();
    bAwaitingInput = true;

    // 黃色高亮
    FSlateBrush Brush;
    Brush.DrawAs   = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = FSlateColor(FLinearColor(1.0f, 0.9f, 0.1f));
    RowBorder->SetBrush(Brush);

    // 讓此 Widget 接收鍵盤事件
    SetFocus();

    if (StatusText)
        StatusText->SetText(FText::FromString(TEXT("請按下新鍵位（最多 4 鍵，Esc 取消）")));
}

void UInputSettingsWidget::CommitRebind()
{
    bAwaitingInput = false;

    if (PendingBorder)
    {
        FSlateBrush Brush;
        Brush.DrawAs   = ESlateBrushDrawType::RoundedBox;
        Brush.TintColor = FSlateColor(FLinearColor(0.80f, 0.85f, 0.95f));
        PendingBorder->SetBrush(Brush);
    }

    if (PendingKeys.IsEmpty() || !PendingOrigKey.IsValid()) { CancelRebind(); return; }

    // 套用主鍵到 IMC
    if (RemapActionInIMC(PendingActionName, PendingOrigKey, PendingKeys[0]))
    {
        // 寫入 Config（完整組合鍵）
        const FString CfgPath = ConfigFilePath();
        const FString NewRebindId = MakeRebindId(PendingActionName, PendingKeys[0]);
        GConfig->SetString(ConfigSection, *NewRebindId, *KeysToConfigStr(PendingKeys), CfgPath);
        // 若舊 RebindId 不同（主鍵改變），刪除舊 key
        if (NewRebindId != PendingRebindId)
            GConfig->RemoveKey(ConfigSection, *PendingRebindId, CfgPath);
        GConfig->Flush(false, CfgPath);

        if (StatusText)
            StatusText->SetText(FText::FromString(
                TEXT("已更新：") + GetDisplayName(PendingActionName, PendingKeys[0]) +
                TEXT(" → ") + KeysToConfigStr(PendingKeys)));
    }

    PendingBorder = nullptr;
    RebuildRows(GetCurrentBindings());
}

void UInputSettingsWidget::CancelRebind()
{
    bAwaitingInput = false;

    if (PendingBorder)
    {
        FSlateBrush Brush;
        Brush.DrawAs   = ESlateBrushDrawType::RoundedBox;
        Brush.TintColor = FSlateColor(FLinearColor(0.80f, 0.85f, 0.95f));
        PendingBorder->SetBrush(Brush);
        PendingBorder = nullptr;
    }

    PendingKeys.Empty();
    KeysCurrentlyHeld.Empty();
    if (StatusText) StatusText->SetText(FText::GetEmpty());
}

// ── 鍵盤事件捕捉 ─────────────────────────────────────────────────────────────

FReply UInputSettingsWidget::NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& Ev)
{
    if (!bAwaitingInput) return Super::NativeOnKeyDown(Geo, Ev);

    const FKey Key = Ev.GetKey();

    if (Key == EKeys::Escape)
    {
        CancelRebind();
        return FReply::Handled();
    }

    if (!KeysCurrentlyHeld.Contains(Key))
        KeysCurrentlyHeld.Add(Key);

    if (!PendingKeys.Contains(Key) && PendingKeys.Num() < 4)
        PendingKeys.Add(Key);

    return FReply::Handled();
}

FReply UInputSettingsWidget::NativeOnKeyUp(const FGeometry& Geo, const FKeyEvent& Ev)
{
    if (!bAwaitingInput) return Super::NativeOnKeyUp(Geo, Ev);

    KeysCurrentlyHeld.Remove(Ev.GetKey());

    // 所有鍵都鬆開後 commit
    if (KeysCurrentlyHeld.IsEmpty() && PendingKeys.Num() > 0)
        CommitRebind();

    return FReply::Handled();
}

// ── 按鈕回呼 ─────────────────────────────────────────────────────────────────

void UInputSettingsWidget::OnSaveClicked()
{
    SaveBindings();
    if (StatusText) StatusText->SetText(FText::FromString(TEXT("已儲存鍵位設定")));
}

void UInputSettingsWidget::OnResetClicked()
{
    ResetToDefaults();
    if (StatusText) StatusText->SetText(FText::FromString(TEXT("已恢復預設鍵位")));
}

// ── Public API ────────────────────────────────────────────────────────────────

void UInputSettingsWidget::SaveBindings()
{
    UInputMappingContext* IMC = GetDefaultIMC();
    if (!IMC) return;

    const FString CfgPath = ConfigFilePath();
    GConfig->EmptySection(ConfigSection, CfgPath);

    for (const FKeyBindingEntry& E : GetCurrentBindings())
    {
        if (E.BoundKeys.IsEmpty()) continue;
        const FString RebindId = MakeRebindId(E.ActionName, E.BoundKeys[0]);
        GConfig->SetString(ConfigSection, *RebindId, *KeysToConfigStr(E.BoundKeys), CfgPath);
    }

    GConfig->Flush(false, CfgPath);
}

void UInputSettingsWidget::LoadAndApplyBindings()
{
    const FString CfgPath = ConfigFilePath();
    if (!FPaths::FileExists(CfgPath)) return;

    UInputMappingContext* IMC = GetDefaultIMC();
    if (!IMC) return;

    // 快照避免迭代修改
    const TArray<FEnhancedActionKeyMapping> Snapshot = IMC->GetMappings();

    bool bChanged = false;
    for (const FEnhancedActionKeyMapping& M : Snapshot)
    {
        if (!M.Action) continue;

        // 嘗試以 RebindId 讀取
        FString ComboStr;
        const FString RebindId = MakeRebindId(M.Action->GetName(), M.Key);
        if (!GConfig->GetString(ConfigSection, *RebindId, ComboStr, CfgPath))
        {
            // 相容舊格式（只存 ActionName）
            GConfig->GetString(ConfigSection, *M.Action->GetName(), ComboStr, CfgPath);
        }
        if (ComboStr.IsEmpty()) continue;

        TArray<FKey> LoadedKeys = ConfigStrToKeys(ComboStr);
        if (LoadedKeys.IsEmpty() || !LoadedKeys[0].IsValid()) continue;

        const FKey NewPrimary = LoadedKeys[0];
        if (NewPrimary == M.Key) continue;

        UInputAction* Action = const_cast<UInputAction*>(M.Action.Get());
        IMC->UnmapKey(Action, M.Key);
        IMC->MapKey(Action, NewPrimary);
        bChanged = true;
    }

    if (bChanged) RefreshSubsystem();
}

void UInputSettingsWidget::ResetToDefaults()
{
    UInputMappingContext* IMC = GetDefaultIMC();
    if (!IMC) return;

    for (auto& Pair : OriginalKeys)
    {
        UInputAction* Action = FindActionByName(Pair.Key.ToString());
        if (!Action) continue;

        FKey CurKey;
        for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
            if (M.Action.Get() == Action) { CurKey = M.Key; break; }

        if (CurKey.IsValid() && CurKey != Pair.Value)
        {
            IMC->UnmapKey(Action, CurKey);
            IMC->MapKey(Action, Pair.Value);
        }
    }

    RefreshSubsystem();

    const FString CfgPath = ConfigFilePath();
    if (FPaths::FileExists(CfgPath)) IFileManager::Get().Delete(*CfgPath);

    RebuildRows(GetCurrentBindings());
}
