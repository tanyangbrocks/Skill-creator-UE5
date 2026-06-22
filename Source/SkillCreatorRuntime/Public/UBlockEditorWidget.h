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
class UProgressBar;
class USpinBox;
struct FBlockNode;
struct FSpellArray;
struct FSpellGroup;

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

    // Phase 3/5：指向正在編輯的技能整構（Phase 7/8 由 PlayerController/SwitchEditorGroup
    // 設定）。不持有任何「視覺節點」中介物件——卡片清單直接讀寫 Spell->Blocks 這份樹本身。
    // Spell->Blocks 若為空（全新技能整構）會自動初始化成空陣列，確保 Palette 拖拉有東西可插入。
    void SetEditingSpell(FSpellArray* InSpell);
    // 對應 Godot 「結構變動 → 整個重建清單」（AbilityEditorUI.cs 的 _canvas.Changed 處理鏈）。
    // 2026-06-22 修復拖放消失 bug：這個函式可能從 UBlockDropZoneWidget::NativeOnDrop /
    // UBlockCardWidget::OnDeleteClicked 等「正在處理該卡片本身輸入事件」的呼叫鏈中被呼叫。
    // Godot 對應的清單重建用 QueueFree()（C# GDScript 的延遲銷毀，當前 frame 結束才真正刪除），
    // 所以 Godot 可以在 _DropData 回呼裡安全地觸發整個清單重建，即使重建會銷毀呼叫者自身的節點。
    // UE5 的 UPanelWidget::ClearChildren() 是同步銷毀——如果直接呼叫，會在 Slate 的拖放事件
    // 仍在該 widget 呼叫堆疊上時把它整個摧毀，導致拖入的卡片憑空消失（看不到放置結果）。
    // 因此 RebuildList() 改為「排程到下一個 tick 才真正執行」，公開介面不變，呼叫端不需要修改。
    void RebuildList();
    void RefreshStatsPanel();

    // Phase 7：技能組（5 組，Phase 8 由 PlayerController 注入）+ 目前編輯的槽位索引
    void SetSpellGroups(FSpellGroup* InGroups) { SpellGroups = InGroups; RefreshGroupDotHighlight(); }
    void SetActiveSlot(int32 SlotIdx) { ActiveSlotIndex = SlotIdx; }
    int32 GetActiveSlot() const { return ActiveSlotIndex; }

    // 儲存（對應 Godot SaveSpell()）：5 項驗證全過才廣播，Phase 8 PlayerController 接住寫回 Loadout
    DECLARE_DELEGATE_TwoParams(FOnSaveSpell, const FString&, int32)
    FOnSaveSpell OnSaveSpell;

    // Phase 8：給 PlayerController 用的「請求關閉」入口——跟 UI 上的 ← 返回按鈕走同一條
    // TryExitEditor() 未儲存確認流程，E 鍵不開後門繞過確認。
    void RequestClose() { TryExitEditor(); }

protected:
    virtual void NativeOnInitialized() override;

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
    // 垃圾桶（對應 Godot ScriptCanvas.cs:104-134 _trashZone，畫布右下角，拖入卡片即刪除）
    TObjectPtr<class UBlockTrashZoneWidget> TrashZone;

    // Phase 3/5：正在編輯的技能整構（不擁有）+ 解開的積木樹裸指標（指向 Spell->Blocks）
    FSpellArray* CurrentSpell = nullptr;
    TArray<TUniquePtr<FBlockNode>>* CurrentBlocks = nullptr;

    // 2026-06-22 修復拖放消失 bug：RebuildList() 真正的重建工作搬到這裡，由
    // SetTimerForNextTick 延後一個 tick 執行，避免在 NativeOnDrop/OnClicked 呼叫堆疊
    // 仍在使用某個即將被 ClearChildren() 銷毀的子 widget 時就同步摧毀它。
    bool bRebuildPending = false;
    void RebuildListImmediate();

    // ── Phase 6：容器巢狀導覽（對應 Godot AbilityEditorUI.cs:29-34 _navStack）─────
    FSpellArray* RootSpell = nullptr;
    TArray<TPair<FSpellArray*, FString>> NavStack;
    TObjectPtr<UBorder> ConfirmOverlay; // 進入容器效果前/未儲存確認/驗證錯誤共用的彈窗

    // ── Phase 7：儲存/驗證/未儲存確認/技能組切換 ──────────────────
    bool bIsDirty = false;
    FSpellGroup* SpellGroups = nullptr;
    int32 ActiveSlotIndex = 0;
    TObjectPtr<UButton> SaveButton;

    void HandleSaveClicked();
    void ShowValidationErrors(const TArray<FString>& Errors);
    void TryExitEditor();
    void SwitchGroup(int32 NewGroupIndex);
    void RefreshGroupDotHighlight();

    UFUNCTION() void OnSaveButtonClicked();
    UFUNCTION() void OnSpellNameCommitted(const FText& Text, ETextCommit::Type CommitMethod);

    void RefreshHeaderState(); // 對應 Godot RefreshHeaderState：返回鈕文字+麵包屑顯示/隱藏
    FString BuildBreadcrumb() const;
    void EnterContainerEffect(FBlockNode* EngravingBlock);

    // 通用確認彈窗（Phase 6 進入容器效果 / Phase 7 未儲存確認 共用）。
    // 對應 Godot ConfirmationDialog（AbilityEditorUI.cs:363-383 / 264-327）。
    void ShowConfirmDialog(const FString& Title, const FString& Message,
                           const FString& ConfirmLabel, const FString& CancelLabel,
                           TFunction<void()> OnConfirm, TFunction<void()> OnCancel = nullptr);
    // FOnButtonClickedEvent 是 dynamic multicast，不支援 AddLambda，所以待決回呼存成
    // member，固定綁兩個 UFUNCTION 轉呼叫
    TFunction<void()> PendingDialogConfirm;
    TFunction<void()> PendingDialogCancel;
    UFUNCTION() void OnConfirmDialogConfirmClicked();
    UFUNCTION() void OnConfirmDialogCancelClicked();

    // ── Phase 5：右側統計面板 ────────────────────────────────────
    TObjectPtr<UTextBlock>   ApValueLabel;
    TObjectPtr<UProgressBar> ApBar;
    TObjectPtr<USpinBox>     BaseMpSpin;
    TObjectPtr<UVerticalBox> MpBreakdownList;
    TObjectPtr<UTextBlock>   DescriptionLabel;

    void BuildStatsPanel();
    UFUNCTION() void OnBaseMpChanged(float NewValue);

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
