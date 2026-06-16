#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AEnemy.h"
#include "GridPos.h"
#include "AMobSpawnController.generated.h"

class AEnemyManager;
class AVoxelWorldActor;

// ── 生成表條目（對應 Godot MobTableEntry record）────────────────────────
USTRUCT(BlueprintType)
struct FMobTableEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EEnemyType     Type     = EEnemyType::Melee;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESpawnCategory Category = ESpawnCategory::Common;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float          Weight   = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGridPos       AreaCenter;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32          AreaRadius = 0;
};

// ══════════════════════════════════════════════════════════════════════════
//  AMobSpawnController — 仿 Minecraft 動態野怪生成器
//
//  生成邏輯：玩家附近 MinSpawnDist~MaxSpawnDist tile 隨機找洞穴地板生成。
//  Despawn：Common/Area 敵人超過 DespawnHardDist 立即消除；
//            DespawnSoftDist~DespawnHardDist 區間每秒 SoftDespawnRate 機率消除。
//  SpawnRateMultiplier 可即時調整生成速率（技能 / 道具）。
// ══════════════════════════════════════════════════════════════════════════
UCLASS()
class SKILLCREATORRUNTIME_API AMobSpawnController : public AActor
{
    GENERATED_BODY()
public:
    AMobSpawnController();

    // ── 生成環常數（tile 單位）────────────────────────────────────────
    static constexpr int32 MinSpawnDist    = 32;
    static constexpr int32 MaxSpawnDist    = 128;
    static constexpr int32 DespawnHardDist = 256;
    static constexpr int32 DespawnSoftDist = 192;
    static constexpr int32 MaxCommonActive = 8;
    static constexpr float BaseInterval    = 8.f;

    // ── 可編輯生成表 ──────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn")
    TArray<FMobTableEntry> MobTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawn")
    float SpawnRateMultiplier = 1.f;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // MobTable 為空時由此填入預設條目；外部可在 Editor 覆寫 MobTable 取代之
    void PopulateDefaultTable();

private:
    static constexpr int32 SpawnTriesPerAttempt = 24;
    static constexpr float SoftDespawnRate      = 0.05f;

    float SpawnTimer = 0.f;

    UPROPERTY() TObjectPtr<AEnemyManager>    CachedEnemyMgr;
    UPROPERTY() TObjectPtr<AVoxelWorldActor> CachedVoxelWorld;

    void  HandleDespawns(const FGridPos& PlayerPos, float DeltaTime);
    const FMobTableEntry* PickEntry() const;
    bool  TryFindSpawnPos(const FGridPos& Player, const FMobTableEntry& Entry, FGridPos& OutPos) const;

    static float HorizDist(const FGridPos& A, const FGridPos& B);
};
