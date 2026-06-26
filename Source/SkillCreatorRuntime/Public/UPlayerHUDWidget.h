#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "SpellArray.h"
#include "ManaSlot.h"
#include "PlacementShape.h"
#include "ItemId.h"
#include "UPlayerHUDWidget.generated.h"

class UCharacterStateComponent;
class UCanvasPanel;
class UImage;
class UHorizontalBox;

// 玩家常駐 HUD：血條 / 法力 / 生存條 / 熱鍵欄 / 等級 / 準心 / 死亡遮罩
// 所有元素純 C++ 程式化建構，不需要 WBP Blueprint
UCLASS(BlueprintType, Blueprintable)
class SKILLCREATORRUNTIME_API UPlayerHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ── 基本血魔（向後相容）──────────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, Category="HUD|HP") float HpPercent    = 1.f;
    UPROPERTY(BlueprintReadOnly, Category="HUD|HP") FText HpText;
    UPROPERTY(BlueprintReadOnly, Category="HUD|MP") float MpPercent    = 1.f;
    UPROPERTY(BlueprintReadOnly, Category="HUD|MP") FText MpText;
    UPROPERTY(BlueprintReadOnly, Category="HUD|State") float StaminaPercent = 1.f;
    UPROPERTY(BlueprintReadOnly, Category="HUD|State") float MoodPercent    = 0.7f;

    // ── 法術熱鍵欄（Spell slot 文字）────────────────────────────────
    UPROPERTY(BlueprintReadOnly, Category="HUD|Spells") TArray<FText> HotBarNames;
    UPROPERTY(BlueprintReadOnly, Category="HUD|Spells") int32 ActiveSpellIndex = 0;
    UPROPERTY(BlueprintReadOnly, Category="HUD|Spells") TArray<FText> ActiveSlotList;

    // ── Blueprint widget 綁定（可選）────────────────────────────────
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> HpBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> MpBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> StaminaBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> MoodBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock>   HpTextBlock;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock>   MpTextBlock;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> HotBarBox;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> ActiveSlotBox;

    // ── C++ update API（ASkillCreatorHUD 每幀呼叫）───────────────────

    void UpdateHpMp(float Hp, float MaxHp, float Mp, float MaxMp,
                    float StaminaPct, float MoodPct);

    void UpdateSpellHotBar(const TArray<FSpellArray>& HotBar, int32 ActiveIdx);

    // 物品熱鍵欄（10 格）
    void UpdateItemHotbar(const TArray<struct FItemStack>& Slots, int32 ActiveIdx);

    // 副手欄（圓形單槽，熱鍵欄左側）
    void UpdateOffhandSlot(const struct FItemStack& ItemSlot, bool bActive);

    // 生存條（StateComp 資料）
    void UpdateSurvival(const UCharacterStateComponent* State);

    // 等級 HUD
    void UpdateLevelHUD(int32 Level, float Xp, int32 XpReq,
                        const FString& TierName, FLinearColor TierColor);

    // 裝備標籤（單一已組好的字串，2026-06-23 改成動態欄位數後由呼叫端組裝）
    void UpdateEquipLabel(const FString& Summary);

    // 多重法力條（動態條數）
    void UpdateManaSlots(const TArray<FManaSlot>& Slots);

    // 死亡遮罩
    void ShowDeathScreen(bool bVisible);

    // GameMode 綁定此 delegate，死亡遮罩「重生」按鈕按下後觸發
    FSimpleDelegate OnRespawnRequested;

    // 境界突破通知（3 秒後自動淡出）
    void ShowBreakthrough(const FString& TierName, FLinearColor Color);

    // 形狀指示器（左下角）
    void SetActiveShape(EPlacementShape Shape, int32 Radius = 1);

    // 浮動 tooltip（跟隨滑鼠；傳入螢幕座標）
    void ShowFloatTooltip(const FString& Text, FVector2D ScreenPos);
    void HideFloatTooltip();

    UFUNCTION(BlueprintImplementableEvent, Category="HUD|Spells")
    void OnActiveSlotChanged(int32 NewSlot);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geo, float Delta) override;

private:
    // ── 基本血魔 bars（左上角或舊版）────────────────────────────────
    // （保留供舊版 Blueprint 子類使用，NativeOnInitialized 自動建立）

    // ── 副手欄（熱鍵欄左側，圓形單槽）────────────────────────────
    TObjectPtr<UBorder>    OffhandBorder     = nullptr;
    TObjectPtr<UBorder>    OffhandIconBorder = nullptr;
    TObjectPtr<UTextBlock> OffhandCountLabel = nullptr;
    TObjectPtr<UTextBlock> OffhandKeyLabel   = nullptr;

    // ── 物品熱鍵欄（底部）──────────────────────────────────────────
    TArray<TObjectPtr<UBorder>>    ItemSlotBorders;
    TArray<TObjectPtr<UBorder>>    ItemIconBorders;
    TArray<TObjectPtr<UTextBlock>> ItemCountLabels;
    TArray<TObjectPtr<UTextBlock>> ItemKeyLabels;

    UFUNCTION() void OnRespawnButtonClicked();

    // ── HP 文字標籤（左下角）───────────────────────────────────────
    TObjectPtr<UTextBlock> HpLabel = nullptr;

    // ── 生存條（左下角，6 條 + 體溫）───────────────────────────────
    TArray<TObjectPtr<UBorder>>    SurvivalBarFills;
    TArray<TObjectPtr<UTextBlock>> SurvivalValLabels;
    TObjectPtr<UTextBlock>         BodyTempLabel = nullptr;
    float                          SurvivalBarWidth = 74.f;

    // ── 等級 / XP / 境界（左下角）──────────────────────────────────
    TObjectPtr<UTextBlock> LevelLabel    = nullptr;
    TObjectPtr<UTextBlock> TierLabel     = nullptr;
    TObjectPtr<UTextBlock> XpLabel       = nullptr;
    TObjectPtr<UBorder>    XpBarFill     = nullptr;
    float                  XpBarMaxWidth = 200.f;

    // ── 裝備標籤（左下角）──────────────────────────────────────────
    TObjectPtr<UTextBlock> EquipLabel = nullptr;

    // ── 動態法力條（左下角 VBox，由底向上）─────────────────────────
    TObjectPtr<UVerticalBox>       ManaHudVBox      = nullptr;
    TArray<TObjectPtr<UBorder>>    ManaBarFills;
    TArray<TObjectPtr<UTextBlock>> ManaValLabels;
    TArray<FName>                  ManaCachedKeys;

    // ── 形狀指示器（左下角）────────────────────────────────────────
    TObjectPtr<UTextBlock> ShapeLabel = nullptr;

    // ── 準心（中央）────────────────────────────────────────────────
    // (4 個 Border panels 組成 + 形）

    // ── 死亡遮罩（全螢幕）──────────────────────────────────────────
    TObjectPtr<UBorder> DeathScreen = nullptr;

    // ── 境界突破通知（中央，timed）──────────────────────────────────
    TObjectPtr<UTextBlock> BreakthroughLabel = nullptr;
    float                  BreakthroughTimer = 0.f;
    FLinearColor           BreakthroughBaseColor;

    // ── 浮動 Tooltip ────────────────────────────────────────────────
    TObjectPtr<UBorder>    FloatTooltipPanel = nullptr;
    TObjectPtr<UTextBlock> FloatTooltipText  = nullptr;

    // ── 提示文字（底部中央）────────────────────────────────────────
    TObjectPtr<UTextBlock> HintLabel = nullptr;

    // ── 遊戲時鐘（右上角）──────────────────────────────────────────
    TObjectPtr<UTextBlock> ClockText       = nullptr;
    int64                  LastClockMinute = -1;

    // ── 私有建構 helper ─────────────────────────────────────────────
    void BuildCrosshair(UCanvasPanel* Root);
    void BuildOffhandSlot(UCanvasPanel* Root);
    void BuildItemHotbar(UCanvasPanel* Root);
    void BuildSurvivalBars(UCanvasPanel* Root);
    void BuildLevelHud(UCanvasPanel* Root);
    void BuildManaHud(UCanvasPanel* Root);
    void BuildDeathScreenOverlay(UCanvasPanel* Root);
    void BuildFloatTooltip(UCanvasPanel* Root);
    void BuildBreakthroughLabel(UCanvasPanel* Root);
    void BuildHintLabel(UCanvasPanel* Root);
    void BuildClock(UCanvasPanel* Root);

    void RebuildManaRows();

    static FLinearColor ManaColor(FName Key);
    static FString      ManaAbbrev(FName Key);
    static FLinearColor ItemIconColor(EItemId Id);

    // Godot-style pin helper
    static void Pin(UWidget* W, FVector2D Pos, FVector2D Size,
                    FAnchors Anchors = FAnchors(0.f, 0.f),
                    FVector2D Align  = FVector2D::ZeroVector);
};
