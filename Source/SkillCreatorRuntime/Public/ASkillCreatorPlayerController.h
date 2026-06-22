#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ASkillCreatorPlayerController.generated.h"

// 玩家控制器（legacy BindKey 層，處理數字鍵/滾輪/面板開關）。
// Tab / U / I / O / P：由 ASkillCreatorCharacter 的 Enhanced Input 側負責
//   Tab  → CycleCameraMode（4 視角循環）
//   UIOP → 施法組合（HandleSpellInput）
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

    UPROPERTY()
    TObjectPtr<class UBlockEditorWidget> BlockEditorWidget;
    bool bBlockEditorOpen = false;
    bool bSpellListSlotClickBound = false; // SpellListPanel->OnSlotClicked 只需綁一次
    void OpenBlockEditorForSlot(int32 SlotIndex);
    void OnBlockEditorSave(const FString& SpellName, int32 SlotIndex);
    void OnBlockEditorClosed();
    void OnSpellListSlotClicked(int32 SlotIndex);
    // 面板開關
    void OnOpenSettings();       // B
    void OnOpenShapeMenu();      // N
    void OnSpellGroupSwitch();   // V
    void OnOpenInventory();      // Z
    void OnOpenEquipment();      // X
    void OnOpenStats();          // C
    void OnEquipItem();          // Q — 裝備/使用熱鍵格
};
