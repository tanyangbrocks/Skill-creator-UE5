#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SnapshotTypes.h"
#include "USnapshotManager.generated.h"

class ASkillCreatorCharacter;
class AEnemy;

// 世界快照 stack（對應 Godot SnapshotManager.cs）
// TakeSnapshot 推入；ApplyLatest 彈出並還原所有實體狀態
UCLASS()
class SKILLCREATORRUNTIME_API USnapshotManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    void  TakeSnapshot(ASkillCreatorCharacter* Player, const TArray<AEnemy*>& Enemies);
    bool  ApplyLatest (ASkillCreatorCharacter* Player, const TArray<AEnemy*>& Enemies);

    void  Clear()      { SnapshotStack.Empty(); }
    int32 StackDepth() const { return SnapshotStack.Num(); }

private:
    TArray<FWorldSnapshot> SnapshotStack;
};
