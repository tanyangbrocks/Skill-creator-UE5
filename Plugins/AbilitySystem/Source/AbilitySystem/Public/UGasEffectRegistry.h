#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayEffect.h"
#include "UGasEffectRegistry.generated.h"

// GAS-4: GameInstance subsystem mapping StatusTagLeaf ("Burn", "Frozen" …)
// to the corresponding UGameplayEffect Blueprint class loaded from /Game/Abilities/GE_<Leaf>.
// Loaded lazily on first Initialize() — i.e. once per game session.
UCLASS()
class ABILITYSYSTEM_API UGasEffectRegistry : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Returns the GE class for the given leaf name, or nullptr if not found.
    TSubclassOf<UGameplayEffect> Find(FName StatusTagLeaf) const;

    static UGasEffectRegistry* Get(const UObject* WorldCtx);

private:
    UPROPERTY()
    TMap<FName, TSubclassOf<UGameplayEffect>> Registry;
};
