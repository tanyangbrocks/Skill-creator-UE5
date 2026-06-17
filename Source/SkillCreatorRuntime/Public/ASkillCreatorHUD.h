#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UPlayerHUDWidget.h"
#include "PlacementShape.h"
#include "EquipmentSlotType.h"
#include "ASkillCreatorHUD.generated.h"

class USettingsWidget;
class UShapeMenuWidget;
class USpellGroupWidget;
class UStatsWidget;
class UInventoryWidget;
class UEquipmentWidget;

// 玩家 HUD 生命週期管理 + 每幀資料餵送。
// 所有面板 widget 在 BeginPlay() 建立，預設隱藏；各 Toggle*() 切換顯示狀態。
UCLASS()
class SKILLCREATORRUNTIME_API ASkillCreatorHUD : public AHUD
{
    GENERATED_BODY()
public:
    ASkillCreatorHUD();

    // ── 常駐 HUD widget（血條、準心…） ──────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD")
    TSubclassOf<UPlayerHUDWidget> HUDWidgetClass;

    UPROPERTY(BlueprintReadOnly, Category="HUD")
    TObjectPtr<UPlayerHUDWidget> HUDWidget;

    // ── 面板 widget（開關式面板）────────────────────────────────────
    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<USettingsWidget>   SettingsPanel;

    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UShapeMenuWidget>  ShapeMenuPanel;

    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<USpellGroupWidget> SpellGroupPanel;

    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UStatsWidget>      StatsPanel;

    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UInventoryWidget>  InventoryPanel;

    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UEquipmentWidget>  EquipmentPanel;

    // ── 全域放置狀態（PlayerController 切換，VoxelWorld 讀取）───────
    bool bHoldToPlace    = false;
    bool bPerfectRemove  = false;
    EPlacementShape ActiveShape = EPlacementShape::Single;
    int32 PlaceRadius = 1;

    // ── 面板開關 API（PlayerController 呼叫）────────────────────────
    void ToggleSettings();
    void ToggleShapeMenu();
    void ToggleSpellGroup();
    void ToggleStats();
    void ToggleInventory();
    void ToggleEquipment();

    virtual void BeginPlay() override;
    virtual void DrawHUD() override;

private:
    template<typename T>
    TObjectPtr<T> CreatePanel();
};
