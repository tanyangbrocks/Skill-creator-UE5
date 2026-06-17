#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ASkillCreatorPlayerController.generated.h"

// 玩家控制器（EnhancedInput 框架層）。
// M-4：只做輸入綁定存根；施法 → M-5；攝影機 → M-8；生存環境 → M-7。
UCLASS()
class SKILLCREATORRUNTIME_API ASkillCreatorPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    // Switch to a specific hotbar slot; call from Blueprint number-key events.
    UFUNCTION(BlueprintCallable, Category="Input")
    void SetActiveSpellSlot(int32 Idx);

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

private:
    // 滾輪 + 技能槽
    void OnScrollUp();
    void OnScrollDown();
    // Hotbar 1-5
    void OnHotbar1();
    void OnHotbar2();
    void OnHotbar3();
    void OnHotbar4();
    void OnHotbar5();
    // 技能編輯器 (E)
    void OnOpenEditor();
    // 面板開關
    void OnOpenSettings();     // B
    void OnOpenShapeMenu();    // N
    void OnSpellGroupSwitch(); // V
    void OnOpenInventory();    // Z
    void OnOpenEquipment();    // X
    void OnOpenStats();        // C
    void OnEquipItem();        // Q — 裝備/使用熱鍵格
    void OnToggleXray();       // Tab — X-ray 視野切換
};
