#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterStats.h"
#include "UStatsWidget.generated.h"

class UTextBlock;
class UEquipmentComponent;

// 角色數值面板（C 鍵開關）：顯示完整角色屬性
UCLASS()
class SKILLCREATORRUNTIME_API UStatsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Refresh(const FCharacterStats& Stats, float Hp, float MaxHp,
                 float Mp, float MaxMp, int32 Level, FString TierName,
                 const UEquipmentComponent* Equip);

protected:
    virtual void NativeConstruct() override;

private:
    TObjectPtr<UTextBlock> ContentLabel = nullptr;
};
