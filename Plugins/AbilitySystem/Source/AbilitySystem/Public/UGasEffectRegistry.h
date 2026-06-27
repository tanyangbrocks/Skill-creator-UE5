#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayEffect.h"
#include "AbilitySystemComponent.h"
#include "UGasEffectRegistry.generated.h"

// GAS-4/6: GameInstance subsystem mapping StatusTagLeaf ("Burn", "Frozen" …)
// to the corresponding UGameplayEffect Blueprint class loaded from /Game/Abilities/GE_<Leaf>.
// GAS-6 adds C++ application conditions (replaces GAS ApplicationRequirement Blueprints).
UCLASS()
class ABILITYSYSTEM_API UGasEffectRegistry : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Returns the GE class for the given leaf name, or nullptr if not found.
    TSubclassOf<UGameplayEffect> Find(FName StatusTagLeaf) const;

    // GAS-6: returns true if all C++ application conditions pass for TargetASC.
    // Call this before Find() to skip effects whose prerequisites are not met.
    bool CanApply(FName StatusTagLeaf, UAbilitySystemComponent& TargetASC) const;

    static UGasEffectRegistry* Get(const UObject* WorldCtx);

private:
    UPROPERTY()
    TMap<FName, TSubclassOf<UGameplayEffect>> Registry;
};
