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

private:
    // 滾輪：Ctrl+滾 = 縮放；否則切換物品熱鍵欄
    void OnScrollUp();
    void OnScrollDown();
    // 熱鍵欄 0~9（數字鍵 1~9/0，對應 Godot Key1~Key9/Key0）
    void OnHotbar1(); void OnHotbar2(); void OnHotbar3(); void OnHotbar4(); void OnHotbar5();
    void OnHotbar6(); void OnHotbar7(); void OnHotbar8(); void OnHotbar9(); void OnHotbar0();
    // 積木編輯器 (E) — 切換遊戲內積木 overlay
    void OnOpenEditor();

#if WITH_EDITOR
    TSharedPtr<SWidget> BlockEditorOverlay;
    bool bBlockEditorOpen = false;
    void ToggleBlockEditorOverlay();
#endif
    // 面板開關
    void OnOpenSettings();       // B
    void OnOpenShapeMenu();      // N
    void OnSpellGroupSwitch();   // V
    void OnOpenInventory();      // Z
    void OnOpenEquipment();      // X
    void OnOpenStats();          // C
    void OnEquipItem();          // Q — 裝備/使用熱鍵格
};
