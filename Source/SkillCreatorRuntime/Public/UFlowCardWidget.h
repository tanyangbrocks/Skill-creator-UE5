#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UFlowCardWidget.generated.h"

// 角色/世界清單的單一可點擊卡片（對應 Godot GameFlowUI.MakeCharCard / MakeWorldCard，
// GameFlowUI.cs:220-253 / 356-395）：整張卡片可點擊（選取/進入），右側獨立 🗑 刪除鈕。
// 跟 UBlockCardWidget 同一套「Setup() 自己建 WidgetTree」慣例：CreateWidget 後立即呼叫 Setup，
// 點擊回呼用 TFunction 直接捕捉 Id（UButton::OnClicked 是 dynamic multicast，不支援 AddLambda，
// 但本類別擁有自己的 UFUNCTION，可在內部呼叫捕捉到的 TFunction）。
UCLASS()
class SKILLCREATORRUNTIME_API UFlowCardWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    // bShowDeleteButton=false：W-10 角色創建精靈的種族體系泡泡/種族清單卡重用這個類別時，
    // 不需要 🗑 刪除功能，InOnDeleteClicked 在這種情況下可以傳空 lambda（不會被呼叫到）。
    void Setup(const FString& InId, const FText& InLabelText, const FLinearColor& CardColor,
               TFunction<void(const FString&)> InOnClicked,
               TFunction<void(const FString&)> InOnDeleteClicked,
               bool bShowDeleteButton = true);

private:
    FString Id;
    TFunction<void(const FString&)> OnClicked;
    TFunction<void(const FString&)> OnDeleteClicked;

    UFUNCTION() void HandleCardClicked();
    UFUNCTION() void HandleDeleteClicked();
};
