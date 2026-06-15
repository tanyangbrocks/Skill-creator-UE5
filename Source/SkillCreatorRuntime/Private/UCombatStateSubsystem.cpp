#include "UCombatStateSubsystem.h"

void UCombatStateSubsystem::OnSpellCast()
{
    EnterCombat();
    CastCount++;
    IdleTimer = 0.f;
}

void UCombatStateSubsystem::OnPlayerDealtDamage(float Amount)
{
    EnterCombat();
    DamageDealt += Amount;
    IdleTimer = 0.f;
}

void UCombatStateSubsystem::OnPlayerTookDamage()
{
    EnterCombat();
    bTookDamageThisFrame = true;
    IdleTimer = 0.f;
}

void UCombatStateSubsystem::OnEnemyKilled()
{
    if (bInCombat) KillCount++;
}

void UCombatStateSubsystem::Advance(float DeltaTime)
{
    bTookDamageThisFrame = false;
    if (!bInCombat) return;
    IdleTimer += DeltaTime;
    if (IdleTimer >= OutOfCombatTimeout) ExitCombat();
}

void UCombatStateSubsystem::Reset()
{
    bInCombat            = false;
    BattleId             = 0;
    CastCount            = 0;
    DamageDealt          = 0.f;
    KillCount            = 0;
    bTookDamageThisFrame = false;
    IdleTimer            = 0.f;
    // NextBattleId 不重置，保持跨局唯一性
}

void UCombatStateSubsystem::EnterCombat()
{
    if (bInCombat) return;
    bInCombat   = true;
    BattleId    = NextBattleId++;
    CastCount   = 0;
    DamageDealt = 0.f;
    KillCount   = 0;
    IdleTimer   = 0.f;
}

void UCombatStateSubsystem::ExitCombat()
{
    bInCombat = false;
    IdleTimer = 0.f;
}
