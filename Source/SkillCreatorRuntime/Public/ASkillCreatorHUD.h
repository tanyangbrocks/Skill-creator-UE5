#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UPlayerHUDWidget.h"
#include "PlacementShape.h"
#include "EquipmentSlot.h"
#include "MaterialType.h"
#include "ASkillCreatorHUD.generated.h"

class UPlayerPanelWidget;
class USettingsWidget;
class UShapeMenuWidget;
class USpellGroupWidget;
class UStatsWidget;
class UInventoryWidget;
class UEquipmentWidget;
class UInputSettingsWidget;
class USpellListWidget;
class UDebugPaintWidget;
class UChestWidget;
class AChestActor;
class UCraftingPanelWidget;

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
    // G 鍵主面板（Stage 2+）
    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UPlayerPanelWidget> PlayerPanel;

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

    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UInputSettingsWidget> InputSettingsPanel;

    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<USpellListWidget>  SpellListPanel;

    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UDebugPaintWidget> DebugPaintPanel;

    // 寶箱雙欄面板（docs/plan-item-crafting-system.md §六）：不是常駐 Toggle 面板，
    // 由 AChestActor::Interact() 帶著該寶箱的 InventoryComponent 開啟，再按一次右鍵關閉
    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UChestWidget> ChestPanel;
    UPROPERTY() TObjectPtr<AChestActor> CurrentChestActor;
    void OpenChest(AChestActor* Chest);
    void CloseChest();

    // 加工選單面板（docs/plan-item-crafting-system.md §八）：常駐顯示，DrawHUD() 每幀餵資料
    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UCraftingPanelWidget> CraftingPanel;

    // ── 全域放置狀態（PlayerController 切換，VoxelWorld 讀取）───────
    bool bHoldToPlace    = true;   // Godot Main.cs:124 _holdToPlace = true
    bool bPerfectRemove  = true;   // K-5：Godot Main.cs:125 _perfectRemove = true（原 UE5 誤設 false，從未接通）
    EPlacementShape ActiveShape = EPlacementShape::Single;
    int32 PlaceRadius = 1;

    // K-12：F1 開發者筆刷狀態（對應 Godot Main.cs:550 _activePaintMat / brush size）
    EMaterialType ActivePaintMaterial = EMaterialType::Sand;
    int32         PaintBrushRadius    = 2;

    // K-22：游標懸在熱鍵欄格上時阻斷採掘/放置（Godot Main.cs:1198-1199 _mouseOverHotbar）
    // 由 UPlayerHUDWidget::NativeTick 每幀寫入
    bool bMouseOverHotbar = false;

    // ── 面板開關 API（PlayerController 呼叫）────────────────────────
    // 2026-06-25 整合：G → TogglePlayerPanel；R → ToggleInventoryAndEquipment
    void TogglePlayerPanel();           // G 鍵；Stage 2 完成前為 stub
    void ToggleInventoryAndEquipment(); // R 鍵：物品欄 + 裝備欄同時開關
    // 下列方法保留供內部邏輯呼叫（寶箱開啟、Stage 7 清理前保持不刪）
    void ToggleSettings();
    void ToggleShapeMenu();
    void ToggleSpellGroup();
    void ToggleStats();
    void ToggleInventory();
    void ToggleEquipment();
    void ToggleInputSettings();
    void ToggleSpellList();
    void ToggleDebugPaint();

    virtual void BeginPlay() override;
    virtual void DrawHUD() override;

private:
    template<typename T>
    TObjectPtr<T> CreatePanel();
};
