#pragma once
#include "CoreMinimal.h"
#include "ManaSlot.generated.h"

// 法力插槽：每種法力類型各自獨立的 Current / Max / RegenRate 池。
// W-6：角色持有 ActiveManaSlots；slot[0]（"gui_dao"）對應原 CurrentMp 向後相容。
USTRUCT(BlueprintType)
struct FManaSlot
{
    GENERATED_BODY()

    static constexpr float DefaultMax       = 100.f;
    static constexpr float DefaultRegenRate = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana")
    FName  ManaTypeKey;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana")
    float  Current   = DefaultMax;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana")
    float  Max       = DefaultMax;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mana")
    float  RegenRate = DefaultRegenRate;

    FManaSlot() = default;
    FManaSlot(FName Key, float InMax = DefaultMax, float InRegen = DefaultRegenRate)
        : ManaTypeKey(Key), Current(InMax), Max(InMax), RegenRate(InRegen) {}

    bool IsEmpty()               const { return ManaTypeKey.IsNone(); }
    bool HasEnough(float Amount) const { return Current >= Amount; }
    void Consume(float Amount)         { Current = FMath::Max(0.f, Current - Amount); }
    void Tick(float Delta)             { Current = FMath::Min(Max, Current + RegenRate * Delta); }
};
