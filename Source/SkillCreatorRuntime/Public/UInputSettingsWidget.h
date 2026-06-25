#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "UInputSettingsWidget.generated.h"

class UVerticalBox;
class UHorizontalBox;
class UTextBlock;
class UBorder;
class USizeBox;
class UButton;
class UScrollBox;
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class UInputAction;
class UInputSettingsWidget;

// 每個可重綁行的點擊代理（繞過 Dynamic Delegate 不支援 AddLambda 的限制）
UCLASS()
class SKILLCREATORRUNTIME_API UKeyRebindProxy : public UObject
{
    GENERATED_BODY()
public:
    FString RebindId;
    FString ActionName;
    FKey    OrigKey;
    TWeakObjectPtr<UInputSettingsWidget> Owner;
    TWeakObjectPtr<UBorder>             Border;

    UFUNCTION()
    void OnClick();
};

// BoundKeys[0] = 套用到 Enhanced Input IMC 的主鍵；其餘（最多 3 個）是 Config 額外記錄的組合鍵（顯示用）
USTRUCT(BlueprintType)
struct FKeyBindingEntry
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString ActionName;        // IMC action asset 名
    UPROPERTY(BlueprintReadOnly) TArray<FKey> BoundKeys;    // [0]=主鍵, [1..3]=組合鍵
    UPROPERTY(BlueprintReadOnly) bool    bIsCustomized = false;
    UPROPERTY(BlueprintReadOnly) bool    bIsRemappable = true;
};

// 運行時鍵位重綁 Widget（對應 Godot InputBindings.cs）。
// 顯示可捲動的動作→鍵位表格；點擊鍵位格 → 黃框等待 → 同時按下多鍵 → 全鬆開後儲存。
UCLASS()
class SKILLCREATORRUNTIME_API UInputSettingsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="InputSettings")
    TArray<FKeyBindingEntry> GetCurrentBindings() const;

    // UKeyRebindProxy 呼叫；其餘程式碼不需要直接呼叫
    void BeginRebind(const FString& RebindId, const FString& ActionName,
                     const FKey& OrigKey, UBorder* RowBorder);

    UFUNCTION(BlueprintCallable, Category="InputSettings")
    void SaveBindings();
    UFUNCTION(BlueprintCallable, Category="InputSettings")
    void LoadAndApplyBindings();
    UFUNCTION(BlueprintCallable, Category="InputSettings")
    void ResetToDefaults();

protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& Ev) override;
    virtual FReply NativeOnKeyUp(const FGeometry& Geo, const FKeyEvent& Ev)   override;

private:
    // ── Widget 元件 ──────────────────────────────────────────────────
    UPROPERTY() TObjectPtr<UVerticalBox>              BindingListContainer = nullptr;
    UPROPERTY() TArray<TObjectPtr<UKeyRebindProxy>>   RowProxies;  // 防 GC
    UPROPERTY() TObjectPtr<UTextBlock>   StatusText           = nullptr;
    UPROPERTY() TObjectPtr<UBorder>      PendingBorder        = nullptr;

    // action + PrimaryKey 組成 RebindId → Border 映射（讓 BeginRebind 找到對應格）
    TMap<FString, TObjectPtr<UBorder>> ActionBorders;
    // 原始鍵位快照（actionName → 預設主鍵）
    TMap<FName, FKey> OriginalKeys;

    // ── 重綁狀態機 ──────────────────────────────────────────────────
    FString      PendingRebindId;   // "ActionName|OrigKey"
    FString      PendingActionName;
    FKey         PendingOrigKey;    // 原本的 IMC 主鍵，用來 UnmapKey
    TArray<FKey> PendingKeys;       // 本次按下的鍵（最多 4）
    TArray<FKey> KeysCurrentlyHeld; // 目前仍按住的鍵
    bool         bAwaitingInput = false;

    // ── Helper ──────────────────────────────────────────────────────
    void BuildLayout();
    void RebuildRows(const TArray<FKeyBindingEntry>& Entries);
    void CommitRebind();
    void CancelRebind();
    bool RemapActionInIMC(const FString& ActionName,
                          const FKey& OldKey, const FKey& NewKey);

    UFUNCTION() void OnSaveClicked();
    UFUNCTION() void OnResetClicked();

    UInputAction* FindActionByName(const FString& ActionName) const;
    UEnhancedInputLocalPlayerSubsystem* GetInputSub()  const;
    UInputMappingContext*               GetDefaultIMC() const;
    void RefreshSubsystem() const;

    static FString MakeRebindId(const FString& ActionName, const FKey& Key);
    static FString GetDisplayName(const FString& ActionName, const FKey& Key);
    static bool    IsRemappable(const FString& ActionName, const FKey& Key);
    static FString KeysToConfigStr(const TArray<FKey>& Keys);
    static TArray<FKey> ConfigStrToKeys(const FString& S);
    static FString ConfigFilePath();
    static const TCHAR* ConfigSection;
};
