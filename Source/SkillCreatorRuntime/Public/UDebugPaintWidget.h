#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MaterialType.h"
#include "UDebugPaintWidget.generated.h"

class UButton;
class UTextBlock;

// K-12：開發者筆刷面板（F1 開關）。對應 Godot Main.cs:549-599 _paintToolPanel——
// 材質選擇（左鍵塗 / 右鍵清除）+ 筆刷大小。Godot 本身點擊處理是 GD.Print stub，
// UE5 版本選擇做出真實效果（ASkillCreatorCharacter::OnMine/OnPlace 在
// bDebugPaintEnabled 時改走塗繪分支），面板 UI 結構仍對齊 Godot。
UCLASS()
class SKILLCREATORRUNTIME_API UDebugPaintWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    EMaterialType ActiveMaterial = EMaterialType::Sand;
    int32         BrushRadius    = 2;   // 對應 Godot brush size "5"（index 2）

    TDelegate<void(EMaterialType)> OnMaterialSelected;
    TDelegate<void(int32)>         OnBrushRadiusSelected;

protected:
    virtual void NativeConstruct() override;

private:
    static const int32 MatCount   = 6;
    static const int32 BrushCount = 4;

    TObjectPtr<UButton>    MatButtons[MatCount];
    TObjectPtr<UTextBlock> MatButtonTexts[MatCount];
    TObjectPtr<UButton>    BrushButtons[BrushCount];
    TObjectPtr<UTextBlock> BrushButtonTexts[BrushCount];

    UFUNCTION() void OnMat0Clicked();
    UFUNCTION() void OnMat1Clicked();
    UFUNCTION() void OnMat2Clicked();
    UFUNCTION() void OnMat3Clicked();
    UFUNCTION() void OnMat4Clicked();
    UFUNCTION() void OnMat5Clicked();

    UFUNCTION() void OnBrush0Clicked();
    UFUNCTION() void OnBrush1Clicked();
    UFUNCTION() void OnBrush2Clicked();
    UFUNCTION() void OnBrush3Clicked();

    void SelectMaterial(EMaterialType Mat);
    void SelectBrush(int32 Radius);
    void RefreshHighlight();
    static FString MatName(EMaterialType Mat);
};
