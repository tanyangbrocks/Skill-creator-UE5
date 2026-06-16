#pragma once
#include "CoreMinimal.h"
#include "ItemId.h"
#include "EquipmentSlotType.h"
#include "MaterialType.h"
#include "ItemData.generated.h"

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
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EEquipmentSlotType EquipSlot = EEquipmentSlotType::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float              AtkMult   = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float              DefFlat   = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float              MpBonus   = 0.f;

    FItemData() = default;
};
