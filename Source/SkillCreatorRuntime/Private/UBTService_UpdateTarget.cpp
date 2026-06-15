#include "UBTService_UpdateTarget.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameFramework/PlayerController.h"

UBTService_UpdateTarget::UBTService_UpdateTarget()
{
    NodeName    = TEXT("Update Target");
    Interval    = 0.5f;
    RandomDeviation = 0.1f;
    PlayerKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateTarget, PlayerKey), APawn::StaticClass());
}

void UBTService_UpdateTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    UWorld* World = OwnerComp.GetWorld();
    if (!World) return;

    APawn* PlayerPawn = nullptr;
    if (APlayerController* PC = World->GetFirstPlayerController())
        PlayerPawn = PC->GetPawn();

    if (PlayerPawn)
        OwnerComp.GetBlackboardComponent()->SetValueAsObject(PlayerKey.SelectedKeyName, PlayerPawn);
}
