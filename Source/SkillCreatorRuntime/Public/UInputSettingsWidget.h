#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "UInputSettingsWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UButton;
class UScrollBox;
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class UInputAction;

USTRUCT(BlueprintType)
struct FKeyBindingEntry
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString ActionName;
    UPROPERTY(BlueprintReadOnly) FKey    BoundKey;
    UPROPERTY(BlueprintReadOnly) bool    bIsCustomized = false;
};

// 運行時鍵位重綁 Widget（對應 Godot InputBindings.cs）。
// 不需要 Blueprint 子類或 WBP .uasset，
// 直接 CreateWidget<UInputSettingsWidget>(PC, UInputSettingsWidget::StaticClass())。
// 如需客製化外觀，建立 Blueprint 子類並覆寫 BlueprintNativeEvent。
UCLASS()
class SKILLCREATORRUNTIME_API UInputSettingsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="InputSettings")
    TArray<FKeyBindingEntry> GetCurrentBindings() const;
    UFUNCTION(BlueprintCallable, Category="InputSettings")
    bool RemapAction(const FString& ActionName, const FKey& NewKey);
    UFUNCTION(BlueprintCallable, Category="InputSettings")
    void SaveBindings();
    UFUNCTION(BlueprintCallable, Category="InputSettings")
    void LoadAndApplyBindings();
    UFUNCTION(BlueprintCallable, Category="InputSettings")
    void ResetToDefaults();

    // C++ 有預設實作（重建 BindingListContainer 文字列）；Blueprint 子類可覆寫
    UFUNCTION(BlueprintNativeEvent, Category="InputSettings")
    void OnBindingsChanged(const TArray<FKeyBindingEntry>& UpdatedBindings);
    virtual void OnBindingsChanged_Implementation(const TArray<FKeyBindingEntry>& UpdatedBindings);

protected:
    virtual void NativeConstruct() override;

private:
    UPROPERTY() TObjectPtr<UVerticalBox> BindingListContainer = nullptr;
    UPROPERTY() TObjectPtr<UTextBlock>   StatusText           = nullptr;

    UPROPERTY() TMap<FName, FKey> OriginalKeys;

    void BuildLayout();
    UFUNCTION() void OnSaveClicked();
    UFUNCTION() void OnResetClicked();

    UInputAction* FindActionByName(const FString& ActionName) const;
    UEnhancedInputLocalPlayerSubsystem* GetInputSub()  const;
    UInputMappingContext*               GetDefaultIMC() const;
    void RefreshSubsystem() const;
    static FString ConfigFilePath();
    static const TCHAR* ConfigSection;
};
