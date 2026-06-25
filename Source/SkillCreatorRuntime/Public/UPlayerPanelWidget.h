#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UPlayerPanelWidget.generated.h"

class UCanvasPanel;
class UBorder;
class USizeBox;
class UTextBlock;
class UButton;
class UHorizontalBox;
class UVerticalBox;
class UOverlay;

class UStatsWidget;
class USpellListWidget;
class UOccupationWidget;
class UInnerWorldWidget;
class UGuideWidget;
class UCompendiumWidget;
class UPlayerSettingsWidget;

// 玩家個人面板（G 鍵開關）。
// 上方 4 個 Tab：個人資訊面板 / 職業能力 / 技能創建空間 / 內部空間
// 右上角 3 個圓形按鈕：引導 / 圖鑑 / 設定（點擊後切換整個畫面，左上角顯示返回鍵）
UCLASS()
class SKILLCREATORRUNTIME_API UPlayerPanelWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // ASkillCreatorHUD::DrawHUD() 每幀呼叫（僅在面板可見時）
    void RefreshStatsTab(const struct FCharacterStats& Stats,
                         float Hp, float MaxHp, float Mp, float MaxMp,
                         int32 Level, const FString& TierName,
                         const class UEquipmentComponent* Equip);

    // 供 PlayerController / HUD 一次性綁定 delegate
    USpellListWidget*       GetSpellListWidget()   const { return SpellListContent;  }
    UPlayerSettingsWidget*  GetSettingsWidget()    const { return SettingsContent;   }

protected:
    virtual void NativeOnInitialized() override;

private:
    // ── 頁面枚舉 ────────────────────────────────────────────────────
    enum class EPage : uint8
    {
        Stats       = 0,
        Occupation  = 1,
        SpellEditor = 2,
        InnerWorld  = 3,
        Guide       = 4,
        Compendium  = 5,
        Settings    = 6,
    };

    EPage CurrentPage  = EPage::Stats;
    EPage LastTabPage  = EPage::Stats;   // 返回鍵要回到哪個 Tab

    void SwitchPage(EPage Page);
    void UpdateTabHighlight();

    // ── 頂部列：一般模式（Tab + 圓鈕） ─────────────────────────────
    UPROPERTY() TObjectPtr<UHorizontalBox> NormalHeader    = nullptr;
    UPROPERTY() TObjectPtr<USizeBox>       NormalHeaderBox = nullptr; // 控高度 + 顯示/隱藏
    UPROPERTY() TArray<TObjectPtr<UButton>>    TabButtons;
    UPROPERTY() TArray<TObjectPtr<UBorder>>    TabUnderlines;

    // ── 頂部列：子頁模式（返回鍵 + 標題） ──────────────────────────
    UPROPERTY() TObjectPtr<UHorizontalBox> PageHeader    = nullptr;
    UPROPERTY() TObjectPtr<USizeBox>       PageHeaderBox = nullptr; // 控高度 + 顯示/隱藏
    UPROPERTY() TObjectPtr<UButton>        BackButton    = nullptr;
    UPROPERTY() TObjectPtr<UTextBlock>     PageTitle     = nullptr;

    // ── 內容區：7 個頁面，每次只顯示一個 ───────────────────────────
    UPROPERTY() TObjectPtr<UBorder>         ContentAreaBorder = nullptr;
    UPROPERTY() TArray<TObjectPtr<UBorder>> PageContainers;

    // ── 子 Widget（Stage 3 建立）────────────────────────────────────
    UPROPERTY() TObjectPtr<UStatsWidget>          StatsContent      = nullptr;
    UPROPERTY() TObjectPtr<USpellListWidget>      SpellListContent  = nullptr;
    UPROPERTY() TObjectPtr<UOccupationWidget>     OccupationContent = nullptr;
    UPROPERTY() TObjectPtr<UInnerWorldWidget>     InnerWorldContent = nullptr;
    UPROPERTY() TObjectPtr<UGuideWidget>          GuideContent      = nullptr;
    UPROPERTY() TObjectPtr<UCompendiumWidget>     CompendiumContent = nullptr;
    UPROPERTY() TObjectPtr<UPlayerSettingsWidget> SettingsContent   = nullptr;

    // ── 建構 helper ─────────────────────────────────────────────────
    void BuildLayout();
    void BuildNormalHeader(UVerticalBox* Root);
    void BuildPageHeader(UVerticalBox* Root);
    void BuildContentArea(UVerticalBox* Root);
    void CreateSubWidgets();
    static UBorder* MakeCircleButton(UButton*& OutBtn,
                                     const FString& Label,
                                     FLinearColor BgColor);

    // ── Tab/按鈕 UFUNCTION ───────────────────────────────────────────
    UFUNCTION() void OnTabStats();
    UFUNCTION() void OnTabOccupation();
    UFUNCTION() void OnTabSpellEditor();
    UFUNCTION() void OnTabInnerWorld();
    UFUNCTION() void OnGuideClicked();
    UFUNCTION() void OnCompendiumClicked();
    UFUNCTION() void OnSettingsClicked();
    UFUNCTION() void OnBackClicked();
};
