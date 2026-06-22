#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterSaveData.h"
#include "RaceRegistry.h"
#include "UPlayerCreateWidget.generated.h"

class UVerticalBox;
class UHorizontalBox;
class UTextBlock;
class UButton;
class UEditableText;
class UScrollBox;
class UWrapBox;
class UOverlay;
class UStatAllocatorWidget;

// 角色創建 11 步精靈（對應 docs/plan-w10-character-creation.md，使用者規格
// origin text setting/about w - 10 playercreate.txt）。
// 獨立於 UGameFlowWidget 之外的類別：由 UGameFlowWidget::OnCharCreateNavClicked() 建立、
// 加進同一個 ScreenStack Overlay（跟 TitleScreen/CharSelectScreen 等同一套 ShowScreen()
// 機制），完成（確定創建）或取消（步驟 1 按「上一步」）時透過 OnFinished/OnCancelled
// 兩個 TFunction 回呼通知 UGameFlowWidget。
//
// 步驟陣列設計（容易增減/調整順序，見計畫文件 2.2）：所有步驟共用同一套上一步/下一步/標題/
// 快速建立 chrome，只認 Steps 陣列索引，不認步驟本身內容。要加新步驟、刪步驟、換順序，只需要
// 調整 RegisterSteps() 裡 Steps.Add(...) 的呼叫順序/增減項目。
// 純 C++ 結構（不是 USTRUCT）：TFunction 成員不是 UPROPERTY 相容型別，而 Steps 陣列本身
// 也不是 UPROPERTY（不需要被 Blueprint/序列化看到），不需要 UHT 反射這個結構。
struct FPlayerCreateStep
{
    FText Title;
    TFunction<UWidget*()> Build;       // 建立這一步內容（只呼叫一次，建出來後快取在 StepWidgets）
    TFunction<bool()>     CanAdvance;  // 「下一步」是否可按；unset = 永遠可以
    bool bShowQuickCreate = false;     // 這一步要不要顯示右上角「快速建立」
};

UCLASS()
class SKILLCREATORRUNTIME_API UPlayerCreateWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // 完成創建：呼叫端（UGameFlowWidget）負責把完整 FCharacterSaveData 寫進存檔
    TFunction<void(const FCharacterSaveData&)> OnFinished;
    // 取消創建（步驟 1 按「上一步」=放棄，回角色選擇畫面）
    TFunction<void()> OnCancelled;

    // 重置所有暫存創建狀態並跳回步驟 1（給 UGameFlowWidget 在每次「創建角色」入口呼叫，
    // 避免上次取消/完成後的殘留資料汙染下一次創建）
    void ResetAndShowFirstStep();

protected:
    // 用 NativeOnInitialized()，不是 NativeConstruct()——見 CLAUDE.md「新增 UI Widget」/
    // UGameFlowWidget 同樣的修正說明：NativeConstruct() 建 WidgetTree 對第一次 AddToViewport
    // 永遠太晚，第一次顯示必定空畫面。
    virtual void NativeOnInitialized() override;

private:
    void BuildLayout();
    void RegisterSteps();
    void ShowStep(int32 Index);
    void RefreshChrome();

    UFUNCTION() void OnPrevClicked();
    UFUNCTION() void OnNextClicked();
    UFUNCTION() void OnQuickCreateClicked();
    UFUNCTION() void OnWarningOkClicked();

    // ── chrome（所有步驟共用的外層框架）─────────────────────────────────────
    UPROPERTY() TObjectPtr<UOverlay>    StepStack      = nullptr;
    UPROPERTY() TObjectPtr<UTextBlock>  StepTitleText  = nullptr;
    UPROPERTY() TObjectPtr<UButton>     PrevBtn        = nullptr;
    UPROPERTY() TObjectPtr<UButton>     NextBtn        = nullptr;
    UPROPERTY() TObjectPtr<UTextBlock>  NextBtnText    = nullptr;
    UPROPERTY() TObjectPtr<UButton>     QuickCreateBtn = nullptr;

    TArray<FPlayerCreateStep> Steps;
    TArray<TObjectPtr<UWidget>> StepWidgets; // 跟 Steps 一一對應，lazy build 後快取
    int32 CurrentStepIndex  = 0;
    int32 ReturnToStepIndex = -1; // >=0 時，下一步按鈕變成「確認」並跳回這個步驟（資訊卡編輯用）

    // ── 警告彈窗（步驟 7：創建能力點 < 0）────────────────────────────────────
    UPROPERTY() TObjectPtr<UWidget>    WarningOverlay = nullptr;
    UPROPERTY() TObjectPtr<UTextBlock> WarningText    = nullptr;

    // ── 暫存創建狀態（確定創建才一次性寫進 FCharacterSaveData）───────────────
    FString           CreationName;
    int32             CreationPointsTotal = 0;
    int32             CreationPointsSpent = 0;
    FName             SelectedRaceId;
    TMap<FName,int32> BaseStatAllocations; // Key 對應 FRaceRegistry 7 項基礎能力點名稱

    int32 GetRemainingPoints() const { return CreationPointsTotal - CreationPointsSpent; }
    void  ResetCreationState();
    void  DoQuickCreateRandomize();

    // ── 各步驟建構 ───────────────────────────────────────────────────────────
    UWidget* BuildStepName();
    UWidget* BuildStepPoints();
    UWidget* BuildStepRace();
    UWidget* BuildBlankStep();
    UWidget* BuildStepBaseStats();
    UWidget* BuildStepConfirmCard();

    // 步驟 1：名字輸入
    UPROPERTY() TObjectPtr<UEditableText> NameInput = nullptr;

    // 步驟 2：創建能力點總數
    UPROPERTY() TObjectPtr<UTextBlock> PointsTotalLabel = nullptr;
    UFUNCTION() void OnSetMaxPointsClicked();

    // 步驟 3：種族選擇
    UPROPERTY() TObjectPtr<UWrapBox>     RaceSystemBubbles = nullptr;
    UPROPERTY() TObjectPtr<UScrollBox>   RaceListScroll    = nullptr;
    UPROPERTY() TObjectPtr<UVerticalBox> RaceListContainer = nullptr;
    UPROPERTY() TObjectPtr<UTextBlock>   RaceSpentLabel    = nullptr;
    void RefreshRaceSystemBubbles();
    void ShowRaceList(FName SystemId);
    void SelectRace(FName RaceId);

    // 步驟 6：基礎能力點分配
    UPROPERTY() TObjectPtr<UTextBlock> RemainingPointsLabel = nullptr;
    UPROPERTY() TMap<FName, TObjectPtr<UStatAllocatorWidget>> StatRows;
    void RefreshStatRow(FName StatKey);
    void AdjustStat(FName StatKey, int32 Delta);
    // 使用者直接在數字輸入框打字確認後呼叫：clamp 到 [0, 目前值+剩餘點數] 後套用。
    void SetStatValue(FName StatKey, int32 NewValue);

    // 步驟 11：最終資訊卡。「編輯」按鈕只用 4 個固定 UFUNCTION（不是泛用 TFunction）——
    // UButton::OnClicked 是 dynamic multicast 不支援 AddLambda，但這裡只有 4 個固定可編輯項目
    // （名字/創建能力點/種族/基礎能力點，空白流程的步驟不列編輯項，見計畫文件），不需要再造一個
    // 像 UFlowCardWidget 那樣的 TFunction 包裝類別。
    UPROPERTY() TObjectPtr<UVerticalBox> ConfirmCardContainer = nullptr;
    void RefreshConfirmCard();
    void JumpToStepForEdit(int32 StepIndex);

    UFUNCTION() void OnConfirmCreateClicked();
    UFUNCTION() void OnEditNameClicked();
    UFUNCTION() void OnEditPointsClicked();
    UFUNCTION() void OnEditRaceClicked();
    UFUNCTION() void OnEditStatsClicked();
};
