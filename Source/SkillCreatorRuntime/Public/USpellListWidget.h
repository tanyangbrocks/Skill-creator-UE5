#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "USpellListWidget.generated.h"

class ASkillCreatorCharacter;

// Blueprint 可見的技能插槽顯示資訊（精簡版，避免暴露 FSpellArray 複雜型別）
USTRUCT(BlueprintType)
struct FSpellSlotDisplayInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32   SlotIndex           = 0;
    UPROPERTY(BlueprintReadOnly) FString SpellName;
    UPROPERTY(BlueprintReadOnly) bool    bIsEmpty            = true;
    UPROPERTY(BlueprintReadOnly) bool    bIsActiveSlot       = false;
    UPROPERTY(BlueprintReadOnly) FString StructuredDesc;   // 由 FSpellDescriptionGenerator 產生
    UPROPERTY(BlueprintReadOnly) FString ElementName;      // 主要元素中文名
    UPROPERTY(BlueprintReadOnly) float   BaseMpCost         = 0.f;
    UPROPERTY(BlueprintReadOnly) bool    bIsPassive         = false;
};

// SpellList + SpellGroup 圓點 Widget C++ 基底（對應 Godot SpellListUI.cs）
// 繼承此類建立 WBP_SpellList Blueprint；
// Blueprint 在 OnSpellListRefreshed 中渲染技能清單與五組圓點。
UCLASS(Abstract)
class SKILLCREATORRUNTIME_API USpellListWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // ── Blueprint Callable ────────────────────────────────────────────────

    // 重新從玩家 SpellCaster 讀取目前 Loadout 並廣播 OnSpellListRefreshed
    UFUNCTION(BlueprintCallable, Category="SpellList")
    void RefreshSpellList();

    // 取得目前作用中的技能組索引（0~4）
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="SpellList")
    int32 GetActiveGroupIndex() const;

    // 切換至指定技能組（0~4）並刷新
    UFUNCTION(BlueprintCallable, Category="SpellList")
    void SetActiveGroup(int32 GroupIndex);

    // 取得最後一次 Refresh 的快取清單
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="SpellList")
    const TArray<FSpellSlotDisplayInfo>& GetCachedSlots() const { return CachedSlots; }

    // ── Blueprint Implementable Events ────────────────────────────────────

    // 每次 RefreshSpellList() 完成後呼叫
    // Slots: 目前組全部 10 格；ActiveGroup: 目前使用的組索引（0~4）
    UFUNCTION(BlueprintImplementableEvent, Category="SpellList")
    void OnSpellListRefreshed(const TArray<FSpellSlotDisplayInfo>& Slots, int32 ActiveGroup);

    // 玩家鼠標懸停在某個插槽時呼叫（可用於顯示 Tooltip）
    UFUNCTION(BlueprintImplementableEvent, Category="SpellList")
    void OnSlotHovered(const FSpellSlotDisplayInfo& HoveredSlot);

protected:
    virtual void NativeConstruct() override;

private:
    UPROPERTY() TArray<FSpellSlotDisplayInfo> CachedSlots;

    ASkillCreatorCharacter* GetOwnerCharacter() const;
};
