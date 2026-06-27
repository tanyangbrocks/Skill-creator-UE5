#include "UGasEffectRegistry.h"
#include "Engine/GameInstance.h"
#include "GameplayTagContainer.h"

// All 24 leaf names matching the GE_ asset names created in GAS-3.
static const FName GAllStatusLeaves[] = {
    // Debuffs
    TEXT("Burn"),          TEXT("Suffocation"),  TEXT("Wet"),
    TEXT("Frost"),         TEXT("Frozen"),        TEXT("Poison"),
    TEXT("DarkErosion"),   TEXT("InstantDeath"),  TEXT("Paralysis"),
    TEXT("EnergySeal"),    TEXT("SkillSeal"),     TEXT("Bleed"),
    TEXT("Petrify"),       TEXT("Stun"),          TEXT("Cripple"),
    TEXT("Fear"),          TEXT("Terror"),         TEXT("Misfortune"),
    TEXT("MiningFatigue"),
    // Buffs
    TEXT("SuperArmor"),    TEXT("Phase"),          TEXT("Invincible"),
    TEXT("ElemResBasic"),  TEXT("ElemResAdvanced"),
};

void UGasEffectRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
    for (const FName& Leaf : GAllStatusLeaves)
    {
        // Blueprint-generated class path: /Game/Abilities/GE_<Leaf>.GE_<Leaf>_C
        FString ClassPath = FString::Printf(
            TEXT("/Game/Abilities/GE_%s.GE_%s_C"), *Leaf.ToString(), *Leaf.ToString());

        TSubclassOf<UGameplayEffect> GEClass =
            StaticLoadClass(UGameplayEffect::StaticClass(), nullptr, *ClassPath);
        if (GEClass)
        {
            Registry.Add(Leaf, GEClass);
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("UGasEffectRegistry: failed to load GE asset '%s'"), *ClassPath);
        }
    }
    UE_LOG(LogTemp, Log,
        TEXT("UGasEffectRegistry: loaded %d / %d GE classes"),
        Registry.Num(), (int32)UE_ARRAY_COUNT(GAllStatusLeaves));
}

TSubclassOf<UGameplayEffect> UGasEffectRegistry::Find(FName StatusTagLeaf) const
{
    const TSubclassOf<UGameplayEffect>* Found = Registry.Find(StatusTagLeaf);
    return Found ? *Found : nullptr;
}

bool UGasEffectRegistry::CanApply(FName StatusTagLeaf, UAbilitySystemComponent& TargetASC) const
{
    // GAS-6: C++ predicate table — replaces GAS ApplicationRequirement Blueprints.
    // Only tag-based prerequisites listed here (no cross-module AttributeSet access).

    // Frozen requires Frost (frostbite stacks must be present)
    if (StatusTagLeaf == TEXT("Frozen"))
        return TargetASC.HasMatchingGameplayTag(
            FGameplayTag::RequestGameplayTag(TEXT("Status.Debuff.Frost"), false));

    // Terror requires Fear (fear stacks max out before terror triggers)
    if (StatusTagLeaf == TEXT("Terror"))
        return TargetASC.HasMatchingGameplayTag(
            FGameplayTag::RequestGameplayTag(TEXT("Status.Debuff.Fear"), false));

    return true;
}

UGasEffectRegistry* UGasEffectRegistry::Get(const UObject* WorldCtx)
{
    if (!WorldCtx) return nullptr;
    UWorld* World = WorldCtx->GetWorld();
    if (!World) return nullptr;
    UGameInstance* GI = World->GetGameInstance();
    return GI ? GI->GetSubsystem<UGasEffectRegistry>() : nullptr;
}
