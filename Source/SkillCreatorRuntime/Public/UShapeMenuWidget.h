#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlacementShape.h"
#include "UShapeMenuWidget.generated.h"

class UButton;
class UTextBlock;

// 形狀選擇面板（N 鍵開關）：5 種放置形狀
UCLASS()
class SKILLCREATORRUNTIME_API UShapeMenuWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    EPlacementShape ActiveShape = EPlacementShape::Single;

    // 選擇形狀後通知 HUD
    TDelegate<void(EPlacementShape)> OnShapeSelected;
    TDelegate<void()>                OnCloseRequested;

    void SetActiveShape(EPlacementShape Shape);

protected:
    virtual void NativeConstruct() override;

private:
    static const int32 ShapeCount = 5;
    TObjectPtr<UButton>    ShapeButtons[ShapeCount];
    TObjectPtr<UTextBlock> ShapeButtonTexts[ShapeCount];
    TObjectPtr<UTextBlock> CurrentShapeLabel = nullptr;

    UFUNCTION() void OnShape0Clicked();
    UFUNCTION() void OnShape1Clicked();
    UFUNCTION() void OnShape2Clicked();
    UFUNCTION() void OnShape3Clicked();
    UFUNCTION() void OnShape4Clicked();
    UFUNCTION() void OnCloseClicked();

    void SelectShape(EPlacementShape S);
    void RefreshButtonHighlight();
    static FString ShapeName(EPlacementShape S);
};
