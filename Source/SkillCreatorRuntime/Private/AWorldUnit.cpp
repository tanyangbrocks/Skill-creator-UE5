#include "AWorldUnit.h"
#include "AVoxelWorldActor.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

AWorldUnit::AWorldUnit()
{
    PrimaryActorTick.bCanEverTick = false; // 子類依需求覆寫為 true

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    SetRootComponent(MeshComp);
}

void AWorldUnit::BeginPlay()
{
    Super::BeginPlay();

    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVoxelWorldActor::StaticClass(), Found);
    if (Found.Num() > 0)
        CachedVoxelWorld = Cast<AVoxelWorldActor>(Found[0]);
}
