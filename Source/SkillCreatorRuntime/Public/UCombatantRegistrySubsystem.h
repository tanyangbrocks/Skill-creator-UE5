#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GridPos.h"
#include "UCombatantRegistrySubsystem.generated.h"

class ICombatant;

// 全域戰鬥參與者登記表（UWorldSubsystem）。
// 所有 ICombatant 實例在 BeginPlay 時呼叫 Register，在 EndPlay 時呼叫 Unregister。
// 取代「只能查 ABeastCharacter」的 AEnemyManager::GetEnemies() 成為統一的敵我查詢入口。
// AEnemyManager 退為純「Beast 生成控制器」，不再作為戰鬥查詢來源。
UCLASS()
class SKILLCREATORRUNTIME_API UCombatantRegistrySubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    // ICombatant 實例在 BeginPlay / EndPlay 呼叫這兩個方法
    void Register(ICombatant* C);
    void Unregister(ICombatant* C);

    // ── 查詢 ─────────────────────────────────────────────────────
    // 回傳佔用該 tile 且 IsHostile()==true 的 ICombatant（無則 nullptr）
    ICombatant* FindHostileAt(FGridPos Pos) const;

    // 回傳場景中所有 IsHostile()==true 且 IsAlive()==true 的 ICombatant
    TArray<ICombatant*> GetAllHostile() const;

private:
    // raw ptr：BeginPlay→EndPlay 期間保證有效；EndPlay 的 Unregister 確保不懸空
    TArray<ICombatant*> Combatants;

    // UPROPERTY GC 保護：持有對應 AActor 的強引用，防止 ICombatant 指向的 Actor 被 GC
    UPROPERTY()
    TSet<TObjectPtr<AActor>> CombatantActorRefs;
};
