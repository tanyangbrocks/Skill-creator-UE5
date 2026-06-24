#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UCraftingStationSubsystem.generated.h"

class AWorkbenchActor;
class AChestActor;

// 工作臺範圍判定（docs/plan-item-crafting-system.md §七）。比照 UCombatStateSubsystem 模式：
// 維護目前世界所有 AWorkbenchActor 的位置清單，Spawn/Destroy 時註冊/取消註冊，
// 不需要每幀掃描全世界找工作臺。
UCLASS()
class SKILLCREATORRUNTIME_API UCraftingStationSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void RegisterWorkbench(AWorkbenchActor* WB);
    void UnregisterWorkbench(AWorkbenchActor* WB);

    // 規格「距離合成臺 5 個遊戲單位」（WorldScale.h 既有換算慣例：GrainCurrent×TileSizeCm）
    bool HasNearbyWorkbench(const FVector& PlayerLoc, float RadiusGameUnits = 5.f) const;

    // 加工系統消耗素材順序：玩家身上優先，附近寶箱補差額（§八）；同一個「附近放置物」
    // proximity 查詢機制，跟工作臺共用這個 subsystem，不另開一個。
    void RegisterChest(AChestActor* Chest);
    void UnregisterChest(AChestActor* Chest);
    AChestActor* FindNearbyChest(const FVector& PlayerLoc, float RadiusGameUnits = 5.f) const;

    // 2026-06-23：換世界時呼叫（ASkillCreatorGameMode::SpawnWorldAndMobs，跟
    // AVoxelWorldActor::ReinitializeForWorld/AEnemyManager::ClearAllEnemies 同一批清理）。
    // 這個 subsystem 是 UWorldSubsystem，註解原本假設「隨關卡銷毀重建」，但 MainMap
    // 整個遊戲進程內從未真正重新載入過，舊世界放置的工作臺/寶箱不會自動消失。
    void DestroyAllFixtures();

private:
    UPROPERTY() TArray<TObjectPtr<AWorkbenchActor>> Workbenches;
    UPROPERTY() TArray<TObjectPtr<AChestActor>>     Chests;
};
