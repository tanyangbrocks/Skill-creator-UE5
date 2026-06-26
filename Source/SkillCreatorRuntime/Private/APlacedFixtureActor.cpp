#include "APlacedFixtureActor.h"
#include "UInventoryComponent.h"
#include "ItemId.h"
#include "ItemRegistry.h"
#include "ASkillCreatorCharacter.h"
#include "ASkillCreatorHUD.h"
#include "UCraftingStationSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "WorldScale.h"

APlacedFixtureActor::APlacedFixtureActor()
{
    // MeshComp 由 AWorldUnit 基底建立；此處只設定 Fixture 特有的碰撞與外觀
    MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComp->SetCollisionObjectType(ECC_WorldStatic);
    MeshComp->SetCastShadow(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeFinder.Succeeded())
    {
        MeshComp->SetStaticMesh(CubeFinder.Object);
        // 1 遊戲單位見方（GrainCurrent × TileSizeCm = 100cm）
        const float S = static_cast<float>(WorldScale::GrainCurrent) * WorldScale::TileSizeCm / 100.f;
        MeshComp->SetWorldScale3D(FVector(S));
    }

    InventoryComp = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComp"));
}

void APlacedFixtureActor::Interact_Implementation(ASkillCreatorCharacter* /*Player*/)
{
    // base 預設無動作，子類覆寫
}

FText APlacedFixtureActor::GetInteractPrompt_Implementation() const
{
    return FText::GetEmpty();
}

void AChestActor::BeginPlay()
{
    Super::BeginPlay();
    if (UCraftingStationSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UCraftingStationSubsystem>() : nullptr)
        Sub->RegisterChest(this);
}

void AChestActor::EndPlay(EEndPlayReason::Type Reason)
{
    if (UCraftingStationSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UCraftingStationSubsystem>() : nullptr)
        Sub->UnregisterChest(this);
    Super::EndPlay(Reason);
}

void AChestActor::Interact_Implementation(ASkillCreatorCharacter* Player)
{
    // 開啟雙欄 UI（玩家背包+寶箱背包並排）——物品系統 5（UChestWidget）實際接上
    OpenChestUI(Player);
}

FText AChestActor::GetInteractPrompt_Implementation() const
{
    return INVTEXT("按右鍵開啟寶箱");
}

void AChestActor::OpenChestUI(ASkillCreatorCharacter* Player)
{
    if (!Player) return;
    APlayerController* PC = Cast<APlayerController>(Player->GetController());
    if (ASkillCreatorHUD* HUD = PC ? Cast<ASkillCreatorHUD>(PC->GetHUD()) : nullptr)
        HUD->OpenChest(this);
}

void AWorkbenchActor::BeginPlay()
{
    Super::BeginPlay();
    RegisterWorkbench();
}

void AWorkbenchActor::EndPlay(EEndPlayReason::Type Reason)
{
    UnregisterWorkbench();
    Super::EndPlay(Reason);
}

void AWorkbenchActor::RegisterWorkbench()
{
    if (UCraftingStationSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UCraftingStationSubsystem>() : nullptr)
        Sub->RegisterWorkbench(this);
}

void AWorkbenchActor::UnregisterWorkbench()
{
    if (UCraftingStationSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UCraftingStationSubsystem>() : nullptr)
        Sub->UnregisterWorkbench(this);
}

void AWorkbenchActor::Interact_Implementation(ASkillCreatorCharacter* /*Player*/)
{
    // 規格沒說「對工作臺按右鍵」要做什麼特別的事——工作臺的作用是「附近有工作臺」
    // 的被動判定（UCraftingStationSubsystem），不是互動開菜單，故留空。
}

FText AWorkbenchActor::GetInteractPrompt_Implementation() const
{
    return FText::GetEmpty();
}

void FItemRegistryPlaceableActors::RegisterAll()
{
    FItemRegistry::SetPlaceAsActor(EItemId::PlaceableWoodChest,     AChestActor::StaticClass());
    FItemRegistry::SetPlaceAsActor(EItemId::PlaceableWoodWorkbench, AWorkbenchActor::StaticClass());
}
