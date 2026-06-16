#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AEnemy.h"
#include "AEnemyManager.generated.h"

// 管理場景中所有敵人的生成、死亡、爆炸傷害（對應 Godot EnemyManager.cs）。
// M-4：基礎框架；投射物管理 → M-5；MobSpawnController → M-5。
UCLASS()
class SKILLCREATORRUNTIME_API AEnemyManager : public AActor
{
    GENERATED_BODY()
public:
    AEnemyManager();

    // ── 環境傷害常數（對應 Godot EnemyManager.cs §486）────────────────
    static constexpr float FireDps    = 5.f;   // 玩家站在火焰格每秒受到的傷害
    static constexpr float LavaDps    = 15.f;  // 玩家站在熔岩格每秒受到的傷害
    static constexpr float BoltDamage = 8.f;   // 敵人投射物命中單次傷害

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

    // ── 每幀清理死亡敵人 ──────────────────────────────────────────
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY() TArray<AEnemy*> Enemies;
    int32 DynamicActiveCount = 0;
};
