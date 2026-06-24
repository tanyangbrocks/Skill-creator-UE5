#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemId.h"
#include "EquipmentSlot.h"
#include "MaterialType.h"
#include "ItemData.generated.h"

// 工具類別（對應 docs/plan-item-crafting-system.md §四：木鏟/木斧/木鎬只對對應材質類別加速）
UENUM(BlueprintType)
enum class EToolCategory : uint8
{
    None,
    Shovel,
    Axe,
    Pickaxe,
};

// 物品定義（對應 Godot ItemData.cs record）
// IsPlaceable=true 時 PlaceAs 指定放置材質（None = 不可放置）
USTRUCT(BlueprintType)
struct SKILLCREATORCORE_API FItemData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EItemId           Id             = EItemId::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText             DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool              bIsPlaceable   = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EMaterialType     PlaceAs        = EMaterialType::Air;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool              bIsTool        = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32             ToolTier       = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float             MiningSpeedMult = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32             MaxStack       = 99;

    // ── 裝備屬性（非裝備物品保持預設值）───────────────────────────
    // EquipSlot 改用 FName 對應 FEquipmentSlotRegistry 的 Id（NAME_None = 不可裝備），
    // 不再是固定 UENUM——武器不佔裝備欄，攻擊力走 bIsWeapon+AtkMult 即時計算（見下）
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName              EquipSlot = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float              AtkMult   = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float              DefFlat   = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float              MpBonus   = 0.f;

    // ── 分類旗標（2026-06-23 物品系統擴充，docs/plan-item-crafting-system.md §四）──
    // 物品可同時具備多個分類，沿用既有 bool flag 風格而非互斥 enum
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsWeapon     = false;  // ★武器：攻擊力 = Strength * AtkMult
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsMaterial   = false;  // ★材質：地圖採掘掉落物標籤
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsComponent  = false;  // ★素材：可用於加工標籤
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsConsumable = false;  // ★道具：左鍵使用（覆寫長按/短按分派）

    // 工具類別化：只有類別匹配時 MiningSpeedMult 才加成，類別不匹配仍可挖但無加速
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EToolCategory ToolCategory = EToolCategory::None;

    // 可放置物的「放成 Actor」分支（寶箱/工作臺等不可塑形可放置物用，跟 PlaceAs(tile) 二選一）。
    // 用泛型 AActor 而不是 APlacedFixtureActor，避免 SkillCreatorCore（低層模組）反過來依賴
    // SkillCreatorRuntime（APlacedFixtureActor 所在模組）；OnPlace() 在 Runtime 端再做實際 Cast。
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AActor> PlaceAsActor = nullptr;

    // 可放置物再細分：可塑形（吃形狀選單）／不可塑形（固定單一 Actor），兩者天然互斥，
    // 不額外存資料，直接由 PlaceAs/PlaceAsActor 兩個既有欄位推導
    bool IsMoldablePlaceable()    const { return bIsPlaceable && PlaceAs != EMaterialType::Air; }
    bool IsNonMoldablePlaceable() const { return bIsPlaceable && PlaceAsActor != nullptr; }

    FItemData() = default;
};
