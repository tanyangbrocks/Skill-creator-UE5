#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "USpellListWidget.generated.h"

class ASkillCreatorCharacter;
class UCanvasPanel;
class UScrollBox;
class UHorizontalBox;
class UBorder;
class UTextBlock;
class UButton;
class USpellCircleWidget;

// SpellSlot 顯示資訊（GetCachedSlots() 的公開 API 保持不變）
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

// 技能創建空間圓球列表（對應 Godot SpellListUI.cs）。
// 不需要 WBP .uasset，直接 CreateWidget<USpellListWidget>(PC, USpellListWidget::StaticClass())。
UCLASS()
class SKILLCREATORRUNTIME_API USpellListWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // ── 公開 API ────────────────────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="SpellList")
    void RefreshSpellList();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="SpellList")
    int32 GetActiveGroupIndex() const;

    UFUNCTION(BlueprintCallable, Category="SpellList")
    void SetActiveGroup(int32 GroupIndex);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="SpellList")
    const TArray<FSpellSlotDisplayInfo>& GetCachedSlots() const { return CachedSlots; }

    // 點擊主動技能圓球 → 通知 PlayerController 切換到積木編輯器（對應 Godot ActiveSpellClicked 信號）
    DECLARE_DELEGATE_OneParam(FOnSlotClicked, int32 /*SlotIndex*/)
    FOnSlotClicked OnSlotClicked;

    // 點擊「+」圓球 → 通知 PlayerController 新增技能（對應 Godot AddSpellRequested 信號）
    DECLARE_DELEGATE(FOnAddSpellRequested)
    FOnAddSpellRequested OnAddSpellRequested;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    // ── 靜態 UI 元素（BuildLayout 建立一次） ────────────────────────────────────
    UPROPERTY() TObjectPtr<UCanvasPanel>   RootCanvas      = nullptr;
    UPROPERTY() TObjectPtr<UHorizontalBox> CircleRow       = nullptr;  // RebuildCircles 動態填入
    UPROPERTY() TObjectPtr<UBorder>        TooltipPanel    = nullptr;
    UPROPERTY() TObjectPtr<UTextBlock>     TooltipLabel    = nullptr;
    UPROPERTY() TObjectPtr<UTextBlock>     MsgLabel        = nullptr;

    // 5 個技能組切換按鈕（標題列同一行，對應 Godot SpellListUI.cs:99-111）
    UPROPERTY() TArray<TObjectPtr<UButton>> GroupDotButtons;

    // ── 快取 ────────────────────────────────────────────────────────────────────
    UPROPERTY() TArray<FSpellSlotDisplayInfo> CachedSlots;
    float MsgTimer = 0.f;

    // ── 內部方法 ─────────────────────────────────────────────────────────────────
    void BuildLayout();
    void RebuildCircles();
    void RefreshGroupDots();
    void ShowMsg(const FString& Msg);
    void ShowTooltip(const FString& Text);
    void HideTooltip();

    ASkillCreatorCharacter* GetOwnerCharacter() const;
    bool IsAtLimit(const struct FSpellLoadout& Loadout) const;
    int32 CountActiveNamed(const struct FSpellLoadout& Loadout) const;

    // 5 個組別按鈕 UFUNCTION（dynamic delegate 不接受 lambda）
    UFUNCTION() void OnGroupBtn0Clicked();
    UFUNCTION() void OnGroupBtn1Clicked();
    UFUNCTION() void OnGroupBtn2Clicked();
    UFUNCTION() void OnGroupBtn3Clicked();
    UFUNCTION() void OnGroupBtn4Clicked();
};
