#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UPlayerHUDWidget.h"
#include "PlacementShape.h"
#include "EquipmentSlot.h"
#include "MaterialType.h"
#include "ASkillCreatorHUD.generated.h"

class UPlayerPanelWidget;
class UShapeMenuWidget;
class USpellGroupWidget;
class UInventoryWidget;
class UEquipmentWidget;
class UDebugPaintWidget;
class UChestWidget;
class AChestActor;
class UCraftingPanelWidget;
class UCraftingHintCardWidget;
class UPhysicalThrowWidget;
class ASkillCreatorCharacter;

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
    // G 鍵主面板（整合 Stats / SpellList / Settings）
    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UPlayerPanelWidget> PlayerPanel;

    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UShapeMenuWidget>  ShapeMenuPanel;

    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<USpellGroupWidget> SpellGroupPanel;

    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UInventoryWidget>  InventoryPanel;

    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UEquipmentWidget>  EquipmentPanel;

    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UDebugPaintWidget> DebugPaintPanel;

    // 寶箱雙欄面板（docs/plan-item-crafting-system.md §六）：不是常駐 Toggle 面板，
    // 由 AChestActor::Interact() 帶著該寶箱的 InventoryComponent 開啟，再按一次右鍵關閉
    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UChestWidget> ChestPanel;
    UPROPERTY() TObjectPtr<AChestActor> CurrentChestActor;
    void OpenChest(AChestActor* Chest);
    void CloseChest();

    // 加工選單提示圖卡（常駐左側，顯示可加工數量）
    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UCraftingHintCardWidget> CraftingHintCard;

    // 加工選單完整面板（Shift 展開/收起，可捲動圖卡列表）
    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UCraftingPanelWidget> CraftingPanel;

    // 投擲力量條（F 長按時顯示，G-7）
    UPROPERTY(BlueprintReadOnly, Category="HUD|Panels")
    TObjectPtr<UPhysicalThrowWidget> ThrowWidget;

    // ── 全域放置狀態（PlayerController 切換，VoxelWorld 讀取）───────
    bool bHoldToPlace    = true;   // Godot Main.cs:124 _holdToPlace = true
    bool bPerfectRemove  = true;   // K-5：Godot Main.cs:125 _perfectRemove = true（原 UE5 誤設 false，從未接通）
    EPlacementShape ActiveShape = EPlacementShape::Single;
    int32 PlaceRadius = 1;

    // K-12：F1 開發者筆刷狀態（對應 Godot Main.cs:550 _activePaintMat / brush size）
    EMaterialType ActivePaintMaterial = EMaterialType::Sand;
    int32         PaintBrushRadius    = 2;

    // ── 面板開關 API（PlayerController 呼叫）────────────────────────
    void TogglePlayerPanel();           // T 鍵（原 G 鍵，2026-06-26 改綁，見 plan-physical-item.md G-0）
    void ToggleInventoryAndEquipment(); // R 鍵：物品欄 + 裝備欄同時開關
    void ToggleShapeMenu();
    void ToggleSpellGroup();
    void ToggleInventory();
    void ToggleEquipment();
    void ToggleDebugPaint();
    void ToggleCraftingPanel();  // Shift 鍵：展開/收起加工選單 + 顯示/隱藏游標

    // G-7/G-8：投擲力量條操作
    void StartThrowCharge();
    void FinishThrowCharge(ASkillCreatorCharacter* Char);  // 停止計時 + 投擲

    virtual void BeginPlay() override;
    virtual void DrawHUD() override;

private:
    // ARC-1：投擲充電中旗標，StartThrowCharge 設 true，FinishThrowCharge 清 false
    bool bIsChargingThrow = false;
    void DrawThrowArc();  // ARC-2：DrawHUD() 呼叫，充電期間繪製拋物線預覽

    template<typename T>
    TObjectPtr<T> CreatePanel();
};
