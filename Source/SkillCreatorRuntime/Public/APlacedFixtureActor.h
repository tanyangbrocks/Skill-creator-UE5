#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IInteractable.h"
#include "APlacedFixtureActor.generated.h"

class UStaticMeshComponent;
class UInventoryComponent;
class ASkillCreatorCharacter;

// 不可塑形可放置物的基底類（寶箱/工作臺共用，docs/plan-item-crafting-system.md §六、七）。
// 持有 UInventoryComponent（重用現有元件，寶箱會用到，工作臺單純不去呼叫它）+
// UStaticMeshComponent（暫用引擎 Cube，跟 AEnemy::MeshComp 同款做法）+ 實作 IInteractable。
UCLASS()
class SKILLCREATORRUNTIME_API APlacedFixtureActor : public AActor, public IInteractable
{
    GENERATED_BODY()
public:
    APlacedFixtureActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fixture")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Fixture")
    TObjectPtr<UInventoryComponent> InventoryComp;

    // IInteractable（base 預設無動作，AChestActor/AWorkbenchActor 各自覆寫）
    virtual void Interact_Implementation(ASkillCreatorCharacter* Player);
    virtual FText GetInteractPrompt_Implementation() const;
};

// 木寶箱：規格「相當於玩家物品欄總數」的格子（UInventoryComponent::TotalSize 固定 30，
// 跟玩家背包同款元件已經自動滿足這個需求，不需要額外設定）。
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

// 木工作臺：不存物品，只是「附近有沒有工作臺」的判定錨點（UCraftingStationSubsystem，
// 物品系統 6 加工系統時接上註冊/取消註冊）。Interact() 規格沒有要求開菜單，留空。
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
    // 物品系統 6（UCraftingStationSubsystem）實作；此處先佔位
    void RegisterWorkbench();
    void UnregisterWorkbench();
};

// 2026-06-23：FItemData.PlaceAsActor 是泛型 TSubclassOf<AActor>（SkillCreatorCore 不能依賴
// SkillCreatorRuntime），這裡提供後置註冊函式，由 SkillCreatorRuntimeModule::StartupModule()
// 呼叫一次，把 AChestActor/AWorkbenchActor 補進 FItemRegistry 對應的 EItemId。
struct FItemRegistryPlaceableActors
{
    static void RegisterAll();
};
