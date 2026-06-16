#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "UInputSettingsWidget.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class UInputAction;

// Blueprint 可見的單項鍵位記錄
USTRUCT(BlueprintType)
struct FKeyBindingEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString ActionName;
    UPROPERTY(BlueprintReadOnly) FKey    BoundKey;
    UPROPERTY(BlueprintReadOnly) bool    bIsCustomized = false; // true = 已被玩家修改
};

// 運行時鍵位重綁 Widget C++ 基底（對應 Godot InputBindings.cs）
// 直接以 UE5.7 UnmapKey/MapKey 修改 DefaultIMC；OriginalKeys 快照用於 ResetToDefaults。
// 繼承此類建立 WBP_InputSettings Blueprint。
UCLASS(Abstract)
class SKILLCREATORRUNTIME_API UInputSettingsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // ── Blueprint Callable ────────────────────────────────────────────────

    // 取得全部 Mapping（含已自訂標記）
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="InputSettings")
    TArray<FKeyBindingEntry> GetCurrentBindings() const;

    // 重綁指定動作（動作名稱 = UInputAction::GetName()）
    UFUNCTION(BlueprintCallable, Category="InputSettings")
    bool RemapAction(const FString& ActionName, const FKey& NewKey);

    // 儲存目前鍵位到 Saved/Config/PlayerInputBindings.ini
    UFUNCTION(BlueprintCallable, Category="InputSettings")
    void SaveBindings();

    // 從 ini 載入並套用
    UFUNCTION(BlueprintCallable, Category="InputSettings")
    void LoadAndApplyBindings();

    // 恢復初始鍵位（快照）並刪除 ini
    UFUNCTION(BlueprintCallable, Category="InputSettings")
    void ResetToDefaults();

    // ── Blueprint Implementable ───────────────────────────────────────────
    UFUNCTION(BlueprintImplementableEvent, Category="InputSettings")
    void OnBindingsChanged(const TArray<FKeyBindingEntry>& UpdatedBindings);

protected:
    virtual void NativeConstruct() override;

private:
    // NativeConstruct 時拍攝的原始鍵位快照（ActionName → OriginalKey）
    UPROPERTY() TMap<FName, FKey> OriginalKeys;

    UInputAction* FindActionByName(const FString& ActionName) const;
    UEnhancedInputLocalPlayerSubsystem* GetInputSub()   const;
    UInputMappingContext*               GetDefaultIMC()  const;
    void RefreshSubsystem() const;

    static FString ConfigFilePath();
    static const TCHAR* ConfigSection;
};
