#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridPos.h"
#include "ANPCSpawnController.generated.h"

class AVoxelWorldActor;
class ANPCCharacter;

// 生成表條目（docs/plan-base-npc-system.md §六）
USTRUCT(BlueprintType)
struct FNPCTableEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SubtypeId; // 對應 FNPCKindRegistry，目前只會填 "WanderingBard"
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Weight = 1.f;
};

// 仿 AMobSpawnController 結構（同一份「環形範圍生成/超距 despawn」邏輯），不跟敵人共用
// 同一個生成器（docs/plan-base-npc-system.md §九 開放問題 Q3：商人 NPC 之後可能要持久化，
// 敵人沒有這個顧慮，先分開比較不會互相牽絆）。
UCLASS()
class SKILLCREATORRUNTIME_API ANPCSpawnController : public AActor
{
    GENERATED_BODY()
public:
    ANPCSpawnController();

    // 沿用 AMobSpawnController 同一組常數（規格沒有要求遊蕩詩人有不同的生成密度）
    static constexpr int32 MinSpawnDist    = 32;
    static constexpr int32 MaxSpawnDist    = 128;
    static constexpr int32 DespawnHardDist = 256;
    static constexpr int32 DespawnSoftDist = 192;
    static constexpr int32 MaxActiveNPCs   = 4;
    static constexpr float BaseInterval    = 30.f; // NPC 比敵人稀疏，間隔拉長

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn")
    TArray<FNPCTableEntry> NPCTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn")
    float SpawnRateMultiplier = 1.f;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    void PopulateDefaultTable();

private:
    static constexpr int32 SpawnTriesPerAttempt = 24;
    static constexpr float SoftDespawnRate      = 0.05f;

    float SpawnTimer = 0.f;
    int32 NextNPCSerial = 1;

    UPROPERTY() TArray<TObjectPtr<ANPCCharacter>> SpawnedNPCs;
    UPROPERTY() TObjectPtr<AVoxelWorldActor> CachedVoxelWorld;

    void HandleDespawns(const FGridPos& PlayerPos, float DeltaTime);
    const FNPCTableEntry* PickEntry() const;
    bool TryFindSpawnPos(const FGridPos& Player, FGridPos& OutPos) const;

    static float HorizDist(const FGridPos& A, const FGridPos& B);
};
