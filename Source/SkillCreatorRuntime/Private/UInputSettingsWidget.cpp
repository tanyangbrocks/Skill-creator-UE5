#include "UInputSettingsWidget.h"
#include "ASkillCreatorCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"

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

// ─── 初始化 ──────────────────────────────────────────────────────────────────

void UInputSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 拍攝原始鍵位快照（每個動作取第一個 mapping 的 key）
    OriginalKeys.Empty();
    if (UInputMappingContext* IMC = GetDefaultIMC())
        for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
            if (M.Action && !OriginalKeys.Contains(FName(*M.Action->GetName())))
                OriginalKeys.Add(FName(*M.Action->GetName()), M.Key);

    LoadAndApplyBindings();
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

        // 若目前鍵與快照中的原始鍵不同，標記為已自訂
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

    // 找到目前此動作的舊鍵（取第一個 mapping）
    FKey OldKey;
    for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
    {
        if (M.Action.Get() == Action) { OldKey = M.Key; break; }
    }

    // UE5.7 clean API：先移除舊鍵，再加上新鍵
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

        // 找目前 key
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
