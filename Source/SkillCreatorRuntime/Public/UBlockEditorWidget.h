#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BlockType.h"
#include "UBlockEditorWidget.generated.h"

class UBorder;
class UTextBlock;
class UButton;
class UEditableTextBox;
class UHorizontalBox;
class UVerticalBox;
class UScrollBox;
struct FBlockNode;

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

    // 玩家等級（鎖等級判斷用，Phase 8 由 PlayerController 帶入真實值）
    int32 PlayerLevel = 1;

    // Phase 3：指向正在編輯的積木樹（FSpellArray.Blocks 解開的裸指標，Phase 7/8 由
    // PlayerController/SwitchEditorGroup 設定）。不持有任何「視覺節點」中介物件——
    // 卡片清單直接讀寫這份樹本身。
    void SetEditingBlocks(TArray<TUniquePtr<FBlockNode>>* InBlocks) { CurrentBlocks = InBlocks; RebuildList(); }
    void RebuildList();

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

    // Phase 3：正在編輯的積木樹（不擁有，指向 FSpellArray.Blocks）
    TArray<TUniquePtr<FBlockNode>>* CurrentBlocks = nullptr;

    UFUNCTION() void OnBackClicked();
    UFUNCTION() void OnGroupDot0Clicked();
    UFUNCTION() void OnGroupDot1Clicked();
    UFUNCTION() void OnGroupDot2Clicked();
    UFUNCTION() void OnGroupDot3Clicked();
    UFUNCTION() void OnGroupDot4Clicked();

    // ── Phase 2：左側 Palette（三主分頁：技能因子/積木/刻印）─────────
    // 對應 Godot AbilityEditorUI.cs:419-836
    int32 ActiveMainTab = 0;   // 0=技能因子, 1=積木, 2=刻印
    int32 ActiveSubTab  = 0;

    TObjectPtr<UButton>      MainTabButtons[3];
    TObjectPtr<UVerticalBox> SubTabColumn;
    TObjectPtr<UVerticalBox> PaletteContentList;

    void BuildPalette();
    void RebuildSubTabColumn();
    void RebuildPaletteContent();
    void RefreshMainTabHighlight();

    UFUNCTION() void OnMainTab0Clicked();
    UFUNCTION() void OnMainTab1Clicked();
    UFUNCTION() void OnMainTab2Clicked();

    UFUNCTION() void OnSubTab0Clicked();
    UFUNCTION() void OnSubTab1Clicked();
    UFUNCTION() void OnSubTab2Clicked();
    UFUNCTION() void OnSubTab3Clicked();
    UFUNCTION() void OnSubTab4Clicked();
    UFUNCTION() void OnSubTab5Clicked();
    UFUNCTION() void OnSubTab6Clicked();
    UFUNCTION() void OnSubTab7Clicked();
    UFUNCTION() void OnSubTab8Clicked();
    UFUNCTION() void OnSubTab9Clicked();
    UFUNCTION() void OnSubTab10Clicked();
};
