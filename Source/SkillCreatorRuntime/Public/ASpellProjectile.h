#pragma once
#include "CoreMinimal.h"
#include "ABaseProjectile.h"
#include "GridPos.h"
#include "CharacterStats.h"
#include "ASpellProjectile.generated.h"

class ICombatant;
class ASkillCreatorCharacter;
class ABeastCharacter;

// 離散 tile 移動的技能投射物（繼承 ABaseProjectile 通用移動架構）。
// 命中敵人 → OnHitEnemy 回調（ICombatant*）；命中玩家 → B-3 物理傷害管線（含 S-4 彈反）。
// 敵人查詢改用 UCombatantRegistrySubsystem（取代原本的 AEnemyManager::GetEnemies()）。
UCLASS()
class SKILLCREATORRUNTIME_API ASpellProjectile : public ABaseProjectile
{
    GENERATED_BODY()
public:
    ASpellProjectile();

    // 初始化（由 USpellCaster / UBTTask_AttackPlayer 在 Spawn 後立即呼叫）
    void Init(FGridPos InOrigin, FVector InDir, AVoxelWorldActor* InVoxelWorld);

    UPROPERTY(EditAnywhere, Category="Projectile")
    float BaseDamage = 10.f;

    // 命中敵人回調（ICombatant*；由 USpellCaster 設定，玩家技能用）
    TFunction<void(ICombatant*, FGridPos)> OnHitEnemy;

    // 敵人投射物：走 B-3 管線所需資訊
    TWeakObjectPtr<ASkillCreatorCharacter> PlayerTarget;
    FCharacterStats AttackerStats;
    bool bUseAttackerStats = false;
    TWeakObjectPtr<ABeastCharacter> OriginEnemy; // S-4 彈反凍結目標（投射物發射者）

    bool IsAlive() const { return IsValid(this); }

protected:
    // ABaseProjectile interface
    virtual void OnTileEntered(FGridPos NewTile) override;
};
