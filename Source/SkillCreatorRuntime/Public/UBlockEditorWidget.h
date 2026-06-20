#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UBlockEditorWidget.generated.h"

class UBorder;
class UTextBlock;
class UButton;
class UEditableTextBox;
class UHorizontalBox;
class UVerticalBox;
class UScrollBox;

// 積木編輯器主視窗（runtime UMG，取代 Editor-only 的 SBlockEditorWidget/SGraphEditor）。
// 整體版面對齊 Godot AbilityEditorUI.cs:123-145：
//   Header（50px：返回/名稱輸入/麵包屑/5組dot/狀態文字）
//   Body（HBox）：左 220px Palette（Phase 2）+ 中 積木卡片清單（Phase 3）+ 右 175px 統計（Phase 5）
// Phase 1 範圍：只搭骨架版面，不接拖拉/參數/存檔邏輯（見後續 Phase）。
UCLASS()
class SKILLCREATORRUNTIME_API UBlockEditorWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // 關閉請求（對應 Godot TryExitEditor，目前先直接通知，Phase 7 補未儲存確認）
    TDelegate<void()> OnCloseRequested;

protected:
    virtual void NativeConstruct() override;

private:
    void BuildLayout();
    UWidget* BuildHeader();
    UWidget* BuildBody();

    // ── Header ──────────────────────────────────────────────────
    TObjectPtr<UButton>         BackButton;
    TObjectPtr<UEditableTextBox> SpellNameBox;
    TObjectPtr<UTextBlock>      BreadcrumbLabel;
    static const int32 GroupDotCount = 5;
    TObjectPtr<UButton>         GroupDots[GroupDotCount];
    TObjectPtr<UTextBlock>      StatusLabel;

    // ── Body（三欄容器，Phase 2/3/5 填內容）──────────────────────
    TObjectPtr<UBorder>         LeftPanel;
    TObjectPtr<UScrollBox>      CenterScroll;
    TObjectPtr<UVerticalBox>    CenterList;
    TObjectPtr<UBorder>         RightPanel;

    UFUNCTION() void OnBackClicked();
    UFUNCTION() void OnGroupDot0Clicked();
    UFUNCTION() void OnGroupDot1Clicked();
    UFUNCTION() void OnGroupDot2Clicked();
    UFUNCTION() void OnGroupDot3Clicked();
    UFUNCTION() void OnGroupDot4Clicked();
};
