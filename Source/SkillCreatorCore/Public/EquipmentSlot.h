#pragma once
#include "CoreMinimal.h"
#include "EquipmentSlot.generated.h"

// 裝備欄登錄表（取代 EquipmentSlotType.h 的固定 UENUM）。
// 對應 docs/plan-item-crafting-system.md §衝突2：使用者指出裝備欄之後高機率持續加新欄位，
// 改用跟 FRaceRegistry/FManaTypeRegistry 同一套「登錄表」模式——新增欄位只需在 .cpp 的
// Init() 多加一行 Register()，屬於「只改 .cpp 純邏輯」的安全範疇，不需要關閉 Editor 重新
// Rebuild（固定 UENUM 每次加值都要走完整 Rebuild）。
USTRUCT(BlueprintType)
struct FEquipmentSlotDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;          // "Helmet"/"Armor"/"Pants"/"Boots"/"Accessory"
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 SortOrder = 0;   // 裝備欄 UI 排列順序
};

struct SKILLCREATORCORE_API FEquipmentSlotRegistry
{
    // 依 SortOrder 排序回傳全部欄位
    static const TArray<FEquipmentSlotDef>& GetAll();
    static const FEquipmentSlotDef* Find(FName Id);

private:
    static void Init(TArray<FEquipmentSlotDef>& Out);
};
