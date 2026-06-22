#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UBlockDropZoneWidget.generated.h"

class UBorder;
struct FBlockNode;

// 積木卡片之間的插入點（對應 Godot ScratchCanvas.DropZone，ScratchCanvas.cs:926-977）：
// 6px 高，拖拉經過時綠色高亮，放開時把拖拉中的積木插入到這個位置。
UCLASS()
class SKILLCREATORRUNTIME_API UBlockDropZoneWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Setup(TArray<TUniquePtr<FBlockNode>>* InParentList, int32 InInsertAt, TFunction<void()> InOnChanged);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
                                    UDragDropOperation* InOperation) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
                               UDragDropOperation* InOperation) override;

private:
    TObjectPtr<UBorder> Line;
    TArray<TUniquePtr<FBlockNode>>* ParentList = nullptr;
    int32 InsertAt = 0;
    TFunction<void()> OnChanged;
};
