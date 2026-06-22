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
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"

const TCHAR* UInputSettingsWidget::ConfigSection = TEXT("PlayerInputBindings");

// ─── 私有工具 ────────────────────────────────────────────────────────────────

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

// ─── Widget 樹建構 ────────────────────────────────────────────────────────────

void UInputSettingsWidget::BuildLayout()
{
    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
    WidgetTree->RootWidget = Root;

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(FText::FromString(TEXT("─── 按鍵設定 ───")));
    Root->AddChildToVerticalBox(Title);

    // 鍵位清單（可捲動）
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(Scroll))
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    BindingListContainer = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(BindingListContainer);

    // 提示文字
    UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>();
    Hint->SetText(FText::FromString(TEXT("（鍵位重綁請呼叫 RemapAction(ActionName, NewKey)）")));
    Root->AddChildToVerticalBox(Hint);

    // 操作按鈕列
    UHorizontalBox* BtnRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    Root->AddChildToVerticalBox(BtnRow);

    UButton* SaveBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* SaveTxt = WidgetTree->ConstructWidget<UTextBlock>();
    SaveTxt->SetText(FText::FromString(TEXT("儲存")));
    SaveBtn->AddChild(SaveTxt);
    SaveBtn->OnClicked.AddDynamic(this, &UInputSettingsWidget::OnSaveClicked);
    BtnRow->AddChildToHorizontalBox(SaveBtn);

    UButton* ResetBtn = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* ResetTxt = WidgetTree->ConstructWidget<UTextBlock>();
    ResetTxt->SetText(FText::FromString(TEXT("恢復預設")));
    ResetBtn->AddChild(ResetTxt);
    ResetBtn->OnClicked.AddDynamic(this, &UInputSettingsWidget::OnResetClicked);
    BtnRow->AddChildToHorizontalBox(ResetBtn);

    // 狀態文字
    StatusText = WidgetTree->ConstructWidget<UTextBlock>();
    StatusText->SetText(FText::GetEmpty());
    Root->AddChildToVerticalBox(StatusText);
}

// ─── 初始化 ──────────────────────────────────────────────────────────────────

void UInputSettingsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();

    // 拍攝原始鍵位快照（每個動作取第一個 mapping 的 key）
    OriginalKeys.Empty();
    if (UInputMappingContext* IMC = GetDefaultIMC())
        for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
            if (M.Action && !OriginalKeys.Contains(FName(*M.Action->GetName())))
                OriginalKeys.Add(FName(*M.Action->GetName()), M.Key);

    LoadAndApplyBindings();
}

// ─── BlueprintNativeEvent 預設實作 ───────────────────────────────────────────

void UInputSettingsWidget::OnBindingsChanged_Implementation(const TArray<FKeyBindingEntry>& UpdatedBindings)
{
    if (!BindingListContainer) return;
    BindingListContainer->ClearChildren();

    for (const FKeyBindingEntry& E : UpdatedBindings)
    {
        const FString Marker  = E.bIsCustomized ? TEXT("*") : TEXT(" ");
        const FString RowText = FString::Printf(
            TEXT("%s  %-30s  →  %s"), *Marker, *E.ActionName, *E.BoundKey.ToString());

        UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>();
        Row->SetText(FText::FromString(RowText));
        BindingListContainer->AddChildToVerticalBox(Row);
    }
}

// ─── 按鈕回呼 ────────────────────────────────────────────────────────────────

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

// ─── Public API ──────────────────────────────────────────────────────────────

TArray<FKeyBindingEntry> UInputSettingsWidget::GetCurrentBindings() const
{
    TArray<FKeyBindingEntry> Result;
    UInputMappingContext* IMC = GetDefaultIMC();
    if (!IMC) return Result;

    for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
    {
        if (!M.Action) continue;
        FKeyBindingEntry Entry;
        Entry.ActionName = M.Action->GetName();
        Entry.BoundKey   = M.Key;
        if (const FKey* Orig = OriginalKeys.Find(FName(*Entry.ActionName)))
            Entry.bIsCustomized = (M.Key != *Orig);
        Result.Add(Entry);
    }
    return Result;
}

bool UInputSettingsWidget::RemapAction(const FString& ActionName, const FKey& NewKey)
{
    UInputMappingContext* IMC = GetDefaultIMC();
    if (!IMC) return false;

    UInputAction* Action = FindActionByName(ActionName);
    if (!Action) return false;

    FKey OldKey;
    for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
        if (M.Action.Get() == Action) { OldKey = M.Key; break; }

    IMC->UnmapKey(Action, OldKey);
    IMC->MapKey(Action, NewKey);

    RefreshSubsystem();
    OnBindingsChanged(GetCurrentBindings());
    return true;
}

void UInputSettingsWidget::SaveBindings()
{
    UInputMappingContext* IMC = GetDefaultIMC();
    if (!IMC) return;

    const FString CfgPath = ConfigFilePath();
    GConfig->EmptySection(ConfigSection, CfgPath);

    for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
        if (M.Action)
            GConfig->SetString(ConfigSection, *M.Action->GetName(), *M.Key.ToString(), CfgPath);

    GConfig->Flush(false, CfgPath);
}

void UInputSettingsWidget::LoadAndApplyBindings()
{
    const FString CfgPath = ConfigFilePath();
    if (!FPaths::FileExists(CfgPath)) return;

    UInputMappingContext* IMC = GetDefaultIMC();
    if (!IMC) return;

    // Copy the array before iterating: UnmapKey modifies GetMappings()'s backing array
    const TArray<FEnhancedActionKeyMapping> Snapshot = IMC->GetMappings();

    bool bChanged = false;
    for (const FEnhancedActionKeyMapping& M : Snapshot)
    {
        if (!M.Action) continue;
        FString KeyStr;
        if (!GConfig->GetString(ConfigSection, *M.Action->GetName(), KeyStr, CfgPath)) continue;

        FKey LoadedKey(*KeyStr);
        if (!LoadedKey.IsValid() || LoadedKey == M.Key) continue;

        UInputAction* Action = const_cast<UInputAction*>(M.Action.Get());
        IMC->UnmapKey(Action, M.Key);
        IMC->MapKey(Action, LoadedKey);
        bChanged = true;
    }

    if (bChanged)
    {
        RefreshSubsystem();
        OnBindingsChanged(GetCurrentBindings());
    }
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

        if (CurKey != Pair.Value)
        {
            IMC->UnmapKey(Action, CurKey);
            IMC->MapKey(Action, Pair.Value);
        }
    }

    RefreshSubsystem();

    const FString CfgPath = ConfigFilePath();
    if (FPaths::FileExists(CfgPath)) IFileManager::Get().Delete(*CfgPath);

    OnBindingsChanged(GetCurrentBindings());
}
