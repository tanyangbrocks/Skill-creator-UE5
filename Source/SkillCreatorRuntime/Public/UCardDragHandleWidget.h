#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UCardDragHandleWidget.generated.h"

class UBorder;
struct FBlockNode;

// 積木卡片左側 8px 拖拉手把（對應 Godot ScratchCanvas.DragHandle，ScratchCanvas.cs:897-920）。
// 獨立成一個小 widget 是因為拖拉判定只該發生在這個窄條上，不該整張卡片都能拖（跟
// 刪除鈕/參數控制項衝突）；Godot 也是把 DragHandle 獨立成一個 8px Panel 子類達成同樣效果。
UCLASS()
class SKILLCREATORRUNTIME_API UCardDragHandleWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Setup(const FLinearColor& InColor, TArray<TUniquePtr<FBlockNode>>* InParentList, int32 InIndex);

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
                                       UDragDropOperation*& OutOperation) override;

private:
    TObjectPtr<UBorder> Bg;
    FLinearColor HandleColor = FLinearColor::White;
    TArray<TUniquePtr<FBlockNode>>* ParentList = nullptr;
    int32 IndexInParent = -1;
};
