#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UPaletteItemWidget.generated.h"

class UBorder;
class UTextBlock;
class UBlockDragDropOp;

// Palette 單個可拖拉項目（技能因子/積木/刻印三個分頁共用）。
// 對應 Godot 在各 Palette 按鈕的 GuiInput 偵測（AbilityEditorUI.cs:637-645/789-796/825-833）：
// 用 UMG 原生 NativeOnMouseButtonDown→DetectDrag→NativeOnDragDetected 取代 Godot 自家的
// 4px 位移偵測，語意一致（按下不放拖動才開始拖拉）。
UCLASS()
class SKILLCREATORRUNTIME_API UPaletteItemWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Setup(const FText& InLabel, const FLinearColor& InColor, bool bInLocked,
               TFunction<void(UBlockDragDropOp&)> InConfigureDrag);

protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
                                       UDragDropOperation*& OutOperation) override;

private:
    TObjectPtr<UBorder>    Bg;
    TObjectPtr<UTextBlock> LabelText;

    FLinearColor ItemColor = FLinearColor::White;
    bool         bIsLocked = false;
    TFunction<void(UBlockDragDropOp&)> ConfigureDrag;
};
