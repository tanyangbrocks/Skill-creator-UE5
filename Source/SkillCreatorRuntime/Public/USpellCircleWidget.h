#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "USpellCircleWidget.generated.h"

class UButton;

// 技能圓球 Widget（對應 Godot SpellListUI.cs MakeSpellCircle / MakeAddCircle）。
// 用 TFunction 捕捉槽位索引，取代 dynamic delegate 限制（同 UFlowCardWidget 模式）。
// CreateWidget 後立即呼叫 SetupSpell 或 SetupAdd 建立 WidgetTree。
UCLASS()
class SKILLCREATORRUNTIME_API USpellCircleWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // 主動/被動技能圓球（對應 Godot SpellListUI.cs:378-413 MakeSpellCircle）
    void SetupSpell(const FString& SpellName,
                    const FString& TooltipText,
                    bool           bIsPassive,
                    bool           bIsOverLimit,
                    TFunction<void()>              InOnClicked,
                    TFunction<void(const FString&)> InOnHover,
                    TFunction<void()>              InOnUnhover);

    // 「+」新增圓球（對應 Godot SpellListUI.cs:415-436 MakeAddCircle）
    void SetupAdd(TFunction<void()> InOnClicked);

private:
    TFunction<void()>               OnClickCallback;
    TFunction<void(const FString&)> OnHoverCallback;
    TFunction<void()>               OnUnhoverCallback;
    FString                         CachedTooltipText;

    UPROPERTY() TObjectPtr<UButton> CircleBtn = nullptr;

    // UFUNCTION 中繼（dynamic delegate 不支援 AddLambda，所以用 UFUNCTION 接住再呼叫 TFunction）
    UFUNCTION() void HandleClicked();
    UFUNCTION() void HandleHovered();
    UFUNCTION() void HandleUnhovered();

    static FSlateBrush MakeRoundedBrush(const FLinearColor& BgColor,
                                        const FLinearColor& BorderColor,
                                        float               BorderWidth,
                                        float               CornerRadius = 130.f);
};
