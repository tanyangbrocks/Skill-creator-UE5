#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "USpellListWidget.generated.h"

class ASkillCreatorCharacter;
class UVerticalBox;
class UHorizontalBox;
class UTextBlock;

USTRUCT(BlueprintType)
struct FSpellSlotDisplayInfo
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32   SlotIndex     = 0;
    UPROPERTY(BlueprintReadOnly) FString SpellName;
    UPROPERTY(BlueprintReadOnly) bool    bIsEmpty      = true;
    UPROPERTY(BlueprintReadOnly) bool    bIsActiveSlot = false;
    UPROPERTY(BlueprintReadOnly) FString StructuredDesc;
    UPROPERTY(BlueprintReadOnly) FString ElementName;
    UPROPERTY(BlueprintReadOnly) float   BaseMpCost    = 0.f;
    UPROPERTY(BlueprintReadOnly) bool    bIsPassive    = false;
};

// SpellList + SpellGroup 圓點 Widget（對應 Godot SpellListUI.cs）。
// 不需要 Blueprint 子類或 WBP .uasset，
// 直接 CreateWidget<USpellListWidget>(PC, USpellListWidget::StaticClass())。
// 如需客製化外觀，建立 Blueprint 子類並覆寫 BlueprintNativeEvent。
UCLASS()
class SKILLCREATORRUNTIME_API USpellListWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="SpellList")
    void RefreshSpellList();
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="SpellList")
    int32 GetActiveGroupIndex() const;
    UFUNCTION(BlueprintCallable, Category="SpellList")
    void SetActiveGroup(int32 GroupIndex);
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="SpellList")
    const TArray<FSpellSlotDisplayInfo>& GetCachedSlots() const { return CachedSlots; }

    // C++ 有預設實作；Blueprint 子類可覆寫以客製化外觀
    UFUNCTION(BlueprintNativeEvent, Category="SpellList")
    void OnSpellListRefreshed(const TArray<FSpellSlotDisplayInfo>& Slots, int32 ActiveGroup);
    virtual void OnSpellListRefreshed_Implementation(const TArray<FSpellSlotDisplayInfo>& Slots, int32 ActiveGroup);

    UFUNCTION(BlueprintNativeEvent, Category="SpellList")
    void OnSlotHovered(const FSpellSlotDisplayInfo& HoveredSlot);
    virtual void OnSlotHovered_Implementation(const FSpellSlotDisplayInfo& HoveredSlot);

protected:
    virtual void NativeConstruct() override;

private:
    // BuildLayout 時建立，RefreshSpellList 時只更新文字（不重建樹）
    UPROPERTY() TObjectPtr<UHorizontalBox> SlotContainer     = nullptr;
    UPROPERTY() TObjectPtr<UHorizontalBox> GroupDotContainer = nullptr;
    UPROPERTY() TObjectPtr<UTextBlock>     SlotTooltipText   = nullptr;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> SlotTexts;       // 10 個插槽標籤
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> GroupDotTexts;   // 5 個組別圓點

    UPROPERTY() TArray<FSpellSlotDisplayInfo> CachedSlots;

    void BuildLayout();
    ASkillCreatorCharacter* GetOwnerCharacter() const;
};
