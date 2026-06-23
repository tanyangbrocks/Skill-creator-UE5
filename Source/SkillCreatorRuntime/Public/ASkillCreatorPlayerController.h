#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ASkillCreatorPlayerController.generated.h"

// 玩家控制器（legacy BindKey 層，處理數字鍵/滾輪/面板開關）。
// 視角切換 / U / I / O / P：由 ASkillCreatorCharacter 的 Enhanced Input 側負責
//   Ctrl（短按 Tap）→ CycleCameraMode（4 視角循環）
//   UIOP             → 施法組合（HandleSpellInput）
UCLASS()
class SKILLCREATORRUNTIME_API ASkillCreatorPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    // 切換物品熱鍵欄格（對應 Godot Inventory.ActiveHotbarIndex）
    UFUNCTION(BlueprintCallable, Category="Input")
    void SetActiveHotbarIndex(int32 Idx);

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    // 2026-06-22 修復：MainMap World Settings 的 DefaultGameMode 被存成
    // BP_SkillCreatorGameMode_C（Blueprint 子類，繼承自 C++ ASkillCreatorGameMode），
    // 其 CDO 把 HUDClass 覆寫成 BP_SkillCreatorHUD_C → HUDWidgetClass 又指到舊版
    // WBP_PlayerHUD_C（只有 HpBar/MpBar/StaminaBar/MoodBar/HotBarBox 幾個綁定元件，
    // 是 C++ 純建構 HUD 出現前的舊設計，沒有準心、沒有物品熱鍵欄）。
    // UPlayerHUDWidget::NativeOnInitialized() 偵測到 HpBar 已綁定就會直接 Pin 位置後
    // return，永遠不會呼叫 BuildCrosshair()/BuildItemHotbar() 等——準心、物品熱鍵欄
    // 完全不會被建立。
    // World Settings 的 GameMode 覆寫是序列化在 .umap 二進位資料裡，沒有安全的純文字/
    // C++ 方式改回去；改在這裡覆寫 SpawnDefaultHUD()，不管 GameMode CDO（不管是不是
    // Blueprint 子類）設了什麼 HUDClass，永遠強制生成正確的純 C++ ASkillCreatorHUD
    // （其 constructor 已設 HUDWidgetClass = UPlayerHUDWidget::StaticClass()）。
    virtual void SpawnDefaultHUD() override;

private:
    // 滾輪：Ctrl+滾 = 縮放；否則切換物品熱鍵欄
    void OnScrollUp();
    void OnScrollDown();
    // 熱鍵欄 0~9（數字鍵 1~9/0，對應 Godot Key1~Key9/Key0）
    void OnHotbar1(); void OnHotbar2(); void OnHotbar3(); void OnHotbar4(); void OnHotbar5();
    void OnHotbar6(); void OnHotbar7(); void OnHotbar8(); void OnHotbar9(); void OnHotbar0();
    // 積木編輯器 (E) — 對應 Godot Main.cs:1772-1791 ToggleEditor()：E 鍵先開「圓球列表」
    // （USpellListWidget，對應 Godot SpellListUI），不是直接開積木編輯器本體。
    // 2026-06-22 修復導覽流程錯誤：之前 OnOpenEditor 直接呼叫 ToggleBlockEditorOverlay()，
    // 跳過了 Godot 原本「先選技能槽位 / 切換組別的圓球首頁」這一層（Main.cs:1772-1791
    // ToggleEditor 設 _spellList.Visible=true, _editor.Visible=false；只有
    // OnListActiveSpellClicked / OnListPassiveSpellClicked / OnListAddSpellRequested
    // 這些「圓球列表裡點擊」的回呼才會把 _editor.Visible 設 true）。
    void OnOpenEditor();

    // Shift 游標模式：true 時顯示系統游標並凍結鏡頭旋轉，再按 Shift 切回準心模式。
    // 開啟面板（積木編輯器/技能列表）不影響此標記——面板關閉後仍維持此模式的狀態。
    bool bCursorMode = false;
    void ToggleCursorMode();

    UPROPERTY()
    TObjectPtr<class UBlockEditorWidget> BlockEditorWidget;
    bool bBlockEditorOpen = false;
    bool bSpellListSlotClickBound = false; // SpellListPanel->OnSlotClicked 只需綁一次
    void OpenBlockEditorForSlot(int32 SlotIndex);
    void OnBlockEditorSave(const FString& SpellName, int32 SlotIndex);
    void OnBlockEditorClosed();
    void OnSpellListSlotClicked(int32 SlotIndex);
    // 面板開關（2026-06-23 鍵位重整後）
    void OnOpenSettings();       // B
    void OnOpenShapeMenu();      // N
    void OnOpenInventory();      // R（was Z）
    void OnOpenEquipment();      // T（was X）
    void OnOpenStats();          // G（was C）
    // OnOpenEditor 已在上方宣告（V，was E；原本 V 的技能組切換移至編輯器面板 UI）

    // 動作快捷鍵（plan-player-actions.md）
    void OnUsePotion();           // Q — S-6 服用所有藥水袋（PotionBagComp->UseAllBags）
    void OnToggleLockTarget();    // E — S-3 鎖敵切換（TryToggleLockTarget）
    void OnSwitchLockTarget();    // Tab — S-3 循環切換目標（SwitchToNextLockTarget）
    void OnDropCurrentItem();     // F — 丟出當前物品（DroppedItemManager）
    void OnCancelAction();        // H — 取消施法（SpellRunner::PruneAll）
    void OnSprintPressed();        // Z toggle：第一次=進入疾跑（計時1s升超速），第二次=取消
    void OnSprintReleased();       // 放開 Z 不做任何事（保留 binding，避免 IE_Released 無主報錯）
    void OnGuardBreakOrFly();     // K — 空中=飛行；地面對防禦目標=破防 stub；K+L=前衝 stub
    void OnXPressed();             // X IE_Pressed  — 按情境：飛行/空中/蹲/翻滾/滑鏟/快墜
    void OnXReleased();            // X IE_Released — 按情境：EndGuard / EndSlide
    void OnAttackPressed();        // J IE_Pressed  — X 同時按住=PerformGuard；否則 StartChargingAttack
    void OnAttackReleased();       // J IE_Released — ReleaseAttack（輕攻 or 蓄力攻）
    void OnBackDash();             // L — S-8 後撤衝量（PerformBackDash stub）
    void OnOpenPotionPanel();      // Y — S-6 藥水袋面板（PotionBagComp->TogglePanel stub）
    void OnOpenMap();              // M — S-7 地圖（MapComp->ToggleMap stub）
};
