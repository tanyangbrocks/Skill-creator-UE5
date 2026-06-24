#include "ADestructibleMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "FVoxelInjector.h"
#include "AVoxelWorldActor.h"
#include "Engine/World.h"

ADestructibleMeshActor::ADestructibleMeshActor()
{
    VoxelizationMode = EVoxelizationMode::ForceMeshWithVoxel;

    DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
    RootComponent = DisplayMesh;
    DisplayMesh->SetSimulatePhysics(false);
}

void ADestructibleMeshActor::BeginPlay()
{
    Super::BeginPlay();
    // 啟動時維持 Mesh 顯示（ForceMeshWithVoxel 預設）
}

UVoxelAsset* ADestructibleMeshActor::ResolveAsset() const
{
    if (VoxelAssetDirect) return VoxelAssetDirect;

    // DataTable 查詢（待 DA_VoxelAssetRegistry.uasset 建立後才能查到）
    if (VoxelAssetId.IsNone()) return nullptr;

    UDataTable* Registry = LoadObject<UDataTable>(nullptr,
        TEXT("/Game/Data/DA_VoxelAssetRegistry.DA_VoxelAssetRegistry"));
    if (!Registry) return nullptr;

    const FVoxelAssetEntry* Row = Registry->FindRow<FVoxelAssetEntry>(VoxelAssetId, TEXT(""));
    if (!Row || Row->Asset.IsNull()) return nullptr;

    return Row->Asset.LoadSynchronous();
}

void ADestructibleMeshActor::TriggerDestruction(EDestroyReason Reason, float Intensity,
                                                  FVector SlashDirection)
{
    UVoxelAsset* Asset = ResolveAsset();
    if (!Asset)
    {
        UE_LOG(LogTemp, Warning, TEXT("ADestructibleMeshActor: no VoxelAsset for '%s'"),
               *VoxelAssetId.ToString());
        Destroy();
        return;
    }

    if (DisplayMesh) DisplayMesh->SetVisibility(false);

    AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(GetWorld());
    if (!VW) { Destroy(); return; }

    FTileWorld3D* World = VW->GetTileWorld();
    const int32 WorldH  = VW->WorldHeight;

    FIntVector BoundsMin, BoundsMax;
    FVoxelInjector::Inject(Asset, GetActorTransform(), World, WorldH, BoundsMin, BoundsMax);

    VW->TriggerVoxelDestruction(BoundsMin, BoundsMax, Reason, Intensity, SlashDirection);
    Destroy();
}
