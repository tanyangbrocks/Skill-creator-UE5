#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SnapshotTypes.h"
#include "USnapshotManager.generated.h"

class ASkillCreatorCharacter;
class AEnemy;
class AVoxelWorldActor;

// 世界快照 stack（對應 Godot SnapshotManager.cs）
// TakeSnapshot 推入；ApplyLatest 彈出並還原所有實體狀態 + 可選 tile 世界
UCLASS()
class SKILLCREATORRUNTIME_API USnapshotManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    // TileRadius > 0 時同步快照玩家位置周圍球形 tile（S-13）
    void TakeSnapshot(ASkillCreatorCharacter* Player, const TArray<AEnemy*>& Enemies,
                      AVoxelWorldActor* VoxelWorld = nullptr, int32 TileRadius = 0);

    // 還原最新快照；若有 tile 快照且傳入 VoxelWorld 則同步還原 tiles
    bool ApplyLatest (ASkillCreatorCharacter* Player, const TArray<AEnemy*>& Enemies,
                      AVoxelWorldActor* VoxelWorld = nullptr);

    void  Clear()      { SnapshotStack.Empty(); }
    int32 StackDepth() const { return SnapshotStack.Num(); }

private:
    TArray<FWorldSnapshot> SnapshotStack;
};
