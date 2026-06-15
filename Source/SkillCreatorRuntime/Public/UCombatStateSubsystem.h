#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GridPos.h"
#include "UCombatStateSubsystem.generated.h"

// (座標, 傷害量, 是否玩家受傷)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCombatHitSignature,
    FGridPos, HitPos, float, Damage, bool, bIsPlayerHurt);

// 戰鬥狀態子系統（對應 Godot CombatState.cs §9-B）。
// UGameInstanceSubsystem 取代 static class，跨 Actor 透過 GetSubsystem<> 取用。
UCLASS()
class SKILLCREATORRUNTIME_API UCombatStateSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    static constexpr float OutOfCombatTimeout = 5.f;

    // ── 狀態 ──────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    bool  bInCombat = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    int32 BattleId  = 0;

    // ── 本場統計 ──────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    int32 CastCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    float DamageDealt = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    int32 KillCount = 0;

    // ── 本幀事件 ──────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    bool bTookDamageThisFrame = false;

    // ── 事件委派 ──────────────────────────────────────────────────
    UPROPERTY(BlueprintAssignable, Category="Combat")
    FCombatHitSignature OnHit;

    // ── 事件通知 API（由 SpellCaster / Character 呼叫）────────────
    UFUNCTION(BlueprintCallable, Category="Combat")
    void OnSpellCast();

    UFUNCTION(BlueprintCallable, Category="Combat")
    void OnPlayerDealtDamage(float Amount);

    UFUNCTION(BlueprintCallable, Category="Combat")
    void OnPlayerTookDamage();

    UFUNCTION(BlueprintCallable, Category="Combat")
    void OnEnemyKilled();

    // ── 每幀更新（GameMode / GameState 的 Tick 呼叫）────────────
    UFUNCTION(BlueprintCallable, Category="Combat")
    void Advance(float DeltaTime);

    // ── 場景重啟 ──────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Combat")
    void Reset();

private:
    float IdleTimer   = 0.f;
    int32 NextBattleId = 1;

    void EnterCombat();
    void ExitCombat();
};
