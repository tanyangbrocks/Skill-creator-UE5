#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ICreature.h"
#include "IElementalTarget.h"
#include "CharacterStats.h"
#include "ASkillCreatorCharacter.generated.h"

class UCharacterStateComponent;
class UElementalAuraComponent;
class USpellCaster;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHpChanged, float, NewHp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterDied);

// 玩家角色（對應 Godot PlayerController.cs 角色層）。
// EntityId 固定 -1；HP/MP 從 Stats 初始化。
// 施法（SpellCaster）→ M-5；庫存 → M-7；攝影機 → M-8；生存系統 → M-7。
UCLASS()
class SKILLCREATORRUNTIME_API ASkillCreatorCharacter
    : public ACharacter
    , public ICreature
    , public IElementalTarget
{
    GENERATED_BODY()
public:
    ASkillCreatorCharacter();

    // ── 基礎數值 ──────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats")
    FCharacterStats Stats;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
    float CurrentHp = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
    float CurrentMp = 100.f;

    // ── 組件 ──────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UCharacterStateComponent> StateComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UElementalAuraComponent> AuraComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<USpellCaster> SpellCasterComp;

    // ── 事件 ──────────────────────────────────────────────────────
    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnHpChanged OnHpChanged;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnCharacterDied OnCharacterDied;

    // ── 戰鬥 API ──────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Combat")
    void GainXp(float Amount);

    UFUNCTION(BlueprintCallable, Category="Combat")
    bool IsAliveChar() const { return CurrentHp > 0.f; }

    // ── ICreature ─────────────────────────────────────────────────
    virtual int32    GetCreatureId() const override { return -1; }
    virtual FGridPos GetPosition()   const override;
    virtual float    GetHp()         const override { return CurrentHp; }
    virtual float    GetMaxHp()      const override { return Stats.MaxHpBase; }
    virtual bool     IsAlive()       const override { return CurrentHp > 0.f; }

    // ── IElementalTarget ──────────────────────────────────────────
    virtual int32 GetEntityId()              const override { return -1; }
    virtual void  TakeDirectDamage(float Amount)   override;

    // ── UE5 overrides ─────────────────────────────────────────────
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
};
