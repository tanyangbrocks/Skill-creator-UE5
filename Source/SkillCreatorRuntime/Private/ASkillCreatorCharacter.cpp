#include "ASkillCreatorCharacter.h"
#include "UCharacterStateComponent.h"
#include "UElementalAuraComponent.h"
#include "USpellCaster.h"
#include "UCombatStateSubsystem.h"
#include "GridPos.h"

ASkillCreatorCharacter::ASkillCreatorCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    StateComp       = CreateDefaultSubobject<UCharacterStateComponent>(TEXT("StateComp"));
    AuraComp        = CreateDefaultSubobject<UElementalAuraComponent>(TEXT("AuraComp"));
    SpellCasterComp = CreateDefaultSubobject<USpellCaster>(TEXT("SpellCasterComp"));
}

void ASkillCreatorCharacter::BeginPlay()
{
    Super::BeginPlay();
    CurrentHp = Stats.MaxHpBase;
    CurrentMp = Stats.MaxMpBase;
}

void ASkillCreatorCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // MP 自然回復
    CurrentMp = FMath::Min(Stats.MaxMpBase, CurrentMp + Stats.MpRegenRate * DeltaTime);

    // HP 自然回復（若有設定）
    if (Stats.HpRegenRate > 0.f && CurrentHp > 0.f)
        CurrentHp = FMath::Min(Stats.MaxHpBase, CurrentHp + Stats.HpRegenRate * DeltaTime);

    // CharacterState Tick（體力、精力、心情）
    bool bInCombat = false;
    if (auto* GI = GetWorld()->GetGameInstance())
    if (auto* Subsystem = GI->GetSubsystem<UCombatStateSubsystem>())
        bInCombat = Subsystem->bInCombat;

    float SurvivalDamage = StateComp->TickState(DeltaTime, bInCombat);
    if (SurvivalDamage > 0.f)
        TakeDirectDamage(SurvivalDamage);

    AuraComp->Process(DeltaTime);
}

float ASkillCreatorCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    float Modified = DamageAmount * (1.f + AuraComp->DamageTakenBonus);
    float Final    = FMath::Max(0.f, Modified - Stats.BaseDefense * (1.f - AuraComp->DefensePenalty));
    TakeDirectDamage(Final);
    return Final;
}

void ASkillCreatorCharacter::TakeDirectDamage(float Amount)
{
    if (CurrentHp <= 0.f) return;
    CurrentHp = FMath::Max(0.f, CurrentHp - Amount);
    OnHpChanged.Broadcast(CurrentHp);

    if (auto* GI = GetWorld()->GetGameInstance())
        if (auto* Sub = GI->GetSubsystem<UCombatStateSubsystem>())
            Sub->OnPlayerTookDamage();

    if (CurrentHp <= 0.f)
        OnCharacterDied.Broadcast();
}

FGridPos ASkillCreatorCharacter::GetPosition() const
{
    FVector Loc = GetActorLocation();
    return FGridPos(FMath::RoundToInt(Loc.X), FMath::RoundToInt(Loc.Y), FMath::RoundToInt(Loc.Z));
}

void ASkillCreatorCharacter::GainXp(float Amount)
{
    // M-7 等級系統建立後填入
}
