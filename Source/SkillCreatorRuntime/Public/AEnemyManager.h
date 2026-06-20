#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AEnemy.h"
#include "AEnemyManager.generated.h"

class ASpellProjectile;
class AMobSpawnController;

// 管理場景中所有敵人的生成、死亡、爆炸傷害（對應 Godot EnemyManager.cs）。
// M-4：基礎框架；投射物管理 → M-5；MobSpawnController → M-5。
UCLASS()
class SKILLCREATORRUNTIME_API AEnemyManager : public AActor
{
    GENERATED_BODY()
public:
    AEnemyManager();

    // ── 環境傷害常數（對應 Godot EnemyManager.cs §486）────────────────
    // I-11：修正數值對齊 Godot EnemyManager.cs:486/488/72
    static constexpr float FireDps    = 25.f;  // 玩家站在火焰格每秒受到的傷害（Godot=25，原 5）
    static constexpr float LavaDps    = 40.f;  // 玩家站在熔岩格每秒受到的傷害（Godot=40，原 15）
    static constexpr float BoltDamage = 12.f;  // 敵人投射物命中單次傷害（Godot=12，原 8）

    // ── 生成 ──────────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="EnemyManager")
    AEnemy* Spawn(FGridPos Pos,
                  EEnemyType Type     = EEnemyType::Melee,
                  float      MaxHp    = -1.f,
                  ESpawnCategory Cat  = ESpawnCategory::Common);

    // ── 查詢 ──────────────────────────────────────────────────────
    UFUNCTION(BlueprintPure, Category="EnemyManager")
    const TArray<AEnemy*>& GetEnemies() const { return Enemies; }

    UFUNCTION(BlueprintPure, Category="EnemyManager")
    int32 GetDynamicActiveCount() const { return DynamicActiveCount; }

    // ── 爆炸傷害 ──────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="EnemyManager")
    void ApplyExplosionDamage(FGridPos Center, int32 Radius, float Damage);

    // ── 投射物管理 ────────────────────────────────────────────────
    // EnemyProjectiles 由 Ranged 敵人 Spawn 後注冊，Tick 中清除無效項
    UPROPERTY() TArray<TObjectPtr<ASpellProjectile>> EnemyProjectiles;

    // ── 生成控制器綁定 ────────────────────────────────────────────
    void SetSpawner(AMobSpawnController* Spawner) { CachedSpawner = Spawner; }

    // ── 每幀清理死亡敵人 ──────────────────────────────────────────
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY() TArray<AEnemy*> Enemies;
    int32 DynamicActiveCount = 0;

    UPROPERTY() TObjectPtr<AMobSpawnController> CachedSpawner;

    // 優先用 BP_Enemy（若存在）；找不到時 fallback 到純 C++ AEnemy::StaticClass()。
    // ConstructorHelpers::FClassFinder 只能在 constructor 用，所以在這裡快取結果供 Spawn() 使用。
    UPROPERTY() TSubclassOf<AEnemy> EnemyClassToSpawn;
};
