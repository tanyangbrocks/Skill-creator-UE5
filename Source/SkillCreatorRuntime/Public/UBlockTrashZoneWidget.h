#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UBlockTrashZoneWidget.generated.h"

class UBorder;
class UTextBlock;

// 垃圾桶圖示 — 對應 Godot ScriptCanvas.cs:44-46,103-134 _trashZone（畫布右下角，
// 拖入卡片放開即整串刪除）。Phase 3 的 UE5 重做用線性 UVerticalBox 卡片清單取代
// Godot 的自由畫布，因此這裡的垃圾桶只接受「既有卡片搬移」拖放（UBlockDragDropOp::
// bFromPalette=false + SourceList 有效）：放開時把該節點從原清單移除並呼叫 OnChanged。
// 調色盤新建型拖放（bFromPalette=true）丟到這裡視同取消新增，不做任何事但仍吃下這次拖放
// （回傳 true），避免穿透到底下的卡片清單造成誤插入。
UCLASS()
class SKILLCREATORRUNTIME_API UBlockTrashZoneWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Setup(TFunction<void()> InOnChanged);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
                                    UDragDropOperation* InOperation) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
                               UDragDropOperation* InOperation) override;

private:
    TObjectPtr<UBorder>    Bg;
    TObjectPtr<UTextBlock> Icon;
    TFunction<void()> OnChanged;

    void SetHighlighted(bool bHighlighted);
};
