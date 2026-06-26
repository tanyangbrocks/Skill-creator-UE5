#pragma once
#include "CoreMinimal.h"
#include "AWorldUnit.h"
#include "IInteractable.h"
#include "APlacedFixtureActor.generated.h"

class UInventoryComponent;
class ASkillCreatorCharacter;

// 不可塑形可放置物的基底類（寶箱/工作臺共用，docs/plan-item-crafting-system.md §六、七）。
// MeshComp 由 AWorldUnit 提供；InventoryComp 為 Fixture 專用（工作臺不使用但仍持有）。
UCLASS()
class SKILLCREATORRUNTIME_API APlacedFixtureActor : public AWorldUnit, public IInteractable
{
    GENERATED_BODY()
public:
    APlacedFixtureActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fixture")
    TObjectPtr<UInventoryComponent> InventoryComp;

    // IInteractable（base 預設無動作，AChestActor/AWorkbenchActor 各自覆寫）
    virtual void Interact_Implementation(ASkillCreatorCharacter* Player);
    virtual FText GetInteractPrompt_Implementation() const;
};

// 木寶箱
UCLASS()
class SKILLCREATORRUNTIME_API AChestActor : public APlacedFixtureActor
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    virtual void Interact_Implementation(ASkillCreatorCharacter* Player) override;
    virtual FText GetInteractPrompt_Implementation() const override;

private:
    void OpenChestUI(ASkillCreatorCharacter* Player);
};

// 木工作臺：附近有沒有工作臺的被動判定錨點（UCraftingStationSubsystem）
UCLASS()
class SKILLCREATORRUNTIME_API AWorkbenchActor : public APlacedFixtureActor
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    virtual void Interact_Implementation(ASkillCreatorCharacter* Player) override;
    virtual FText GetInteractPrompt_Implementation() const override;

private:
    void RegisterWorkbench();
    void UnregisterWorkbench();
};

// 2026-06-23：FItemData.PlaceAsActor 是泛型 TSubclassOf<AActor>，此處提供後置註冊。
struct FItemRegistryPlaceableActors
{
    static void RegisterAll();
};
