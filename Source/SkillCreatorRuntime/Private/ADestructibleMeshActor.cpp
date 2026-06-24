#include "ADestructibleMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "FVoxelInjector.h"
#include "VoxParser.h"
#include "AVoxelWorldActor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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
}

bool ADestructibleMeshActor::LoadCells(TArray<FVoxelCell>& OutCells) const
{
    // 備用路徑：Editor 預建的 UVoxelAsset
    if (VoxelAssetDirect && VoxelAssetDirect->Cells.Num() > 0)
    {
        OutCells = VoxelAssetDirect->Cells;
        return true;
    }

    // 主要路徑：runtime 讀 .vox 檔
    if (VoxFilePath.IsEmpty()) return false;

    const FString FullPath = FPaths::ProjectContentDir() / VoxFilePath;
    TArray<uint8> Raw;
    if (!FFileHelper::LoadFileToArray(Raw, *FullPath))
    {
        UE_LOG(LogTemp, Warning,
               TEXT("ADestructibleMeshActor: 找不到 .vox 檔案：%s"), *FullPath);
        return false;
    }

    FIntVector BoundsMax;
    if (!VoxParser::Parse(Raw, OutCells, BoundsMax))
    {
        UE_LOG(LogTemp, Warning,
               TEXT("ADestructibleMeshActor: .vox 解析失敗：%s"), *FullPath);
        return false;
    }

    return OutCells.Num() > 0;
}

void ADestructibleMeshActor::TriggerDestruction(EDestroyReason Reason, float Intensity,
                                                  FVector SlashDirection)
{
    TArray<FVoxelCell> Cells;
    if (!LoadCells(Cells))
    {
        UE_LOG(LogTemp, Warning,
               TEXT("ADestructibleMeshActor '%s': 無體素資料，直接銷毀"),
               *GetName());
        Destroy();
        return;
    }

    if (DisplayMesh) DisplayMesh->SetVisibility(false);

    AVoxelWorldActor* VW = AVoxelWorldActor::FindInWorld(GetWorld());
    if (!VW) { Destroy(); return; }

    FTileWorld3D* World = VW->GetTileWorld();
    const int32   WorldH = VW->WorldHeight;

    FIntVector BoundsMin, BoundsMax;
    FVoxelInjector::Inject(Cells, GetActorTransform(), World, WorldH, BoundsMin, BoundsMax);

    VW->TriggerVoxelDestruction(BoundsMin, BoundsMax, Reason, Intensity, SlashDirection);
    Destroy();
}
