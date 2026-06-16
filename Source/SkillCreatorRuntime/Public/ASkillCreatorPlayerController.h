#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ASkillCreatorPlayerController.generated.h"

// 玩家控制器（EnhancedInput 框架層）。
// M-4：只做輸入綁定存根；施法 → M-5；攝影機 → M-8；生存環境 → M-7。
// M-8：滾輪切換技能槽（legacy BindKey）；Number keys exposed as BlueprintCallable.
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

    // TODO M-5: 施法輸入動作（IA_CastSpell, IA_SwitchSpell…）

private:
    void OnScrollUp();
    void OnScrollDown();
    void OnOpenEditor();
    void OnHotbar1();
    void OnHotbar2();
    void OnHotbar3();
    void OnHotbar4();
    void OnHotbar5();
};
