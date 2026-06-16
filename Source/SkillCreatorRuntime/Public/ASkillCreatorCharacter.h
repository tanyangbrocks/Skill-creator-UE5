#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ICreature.h"
#include "IElementalTarget.h"
#include "CharacterStats.h"
#include "ActionBus.h"
#include "SkillCameraTypes.h"
#include "ManaSlot.h"
#include "ASkillCreatorCharacter.generated.h"

class UCharacterStateComponent;
class UElementalAuraComponent;
class USpellCaster;
class USpringArmComponent;
class UCameraComponent;
class UInventoryComponent;
class UEquipmentComponent;
class AVoxelWorldActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHpChanged, float, NewHp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterDied);

// 玩家角色（對應 Godot PlayerController.cs 角色層）。
// EntityId 固定 -1；HP/MP 從 Stats 初始化。
// 施法（SpellCaster）M-5；庫存 → M-7；生存系統 → M-7。
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

    // W-6: 多重法力插槽；slot[0]（"gui_dao"）與 CurrentMp 保持同步（向後相容）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stats")
    TArray<FManaSlot> ActiveManaSlots;

    // 依 ManaTypeKey 查找插槽；找不到回傳 nullptr
    FManaSlot* GetManaSlot(FName Key);

    // ── 組件 ──────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<UCameraComponent> Camera;

    // ── 攝影機模式（對應 Godot CameraController.cs）──────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    ECameraMode CameraMode = ECameraMode::ThirdPerson;

    // 各模式參數（可覆寫預設值）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    TMap<ECameraMode, FCameraModeParams> CameraPresets;

    // 切換至下一個攝影機模式（循環）
    UFUNCTION(BlueprintCallable, Category="Camera")
    void CycleCameraMode();

    // 切換至指定模式並套用
    UFUNCTION(BlueprintCallable, Category="Camera")
    void SetCameraMode(ECameraMode NewMode);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UCharacterStateComponent> StateComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UElementalAuraComponent> AuraComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<USpellCaster> SpellCasterComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UInventoryComponent> InventoryComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UEquipmentComponent> EquipmentComp;

    // ── 事件 ──────────────────────────────────────────────────────
    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnHpChanged OnHpChanged;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnCharacterDied OnCharacterDied;

    // ── ActionBus（DamageShield / DeathGuard 攔截，DeltaTime 計時，pause-aware）──
    FActionBus ActionBus;

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
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

private:
    void MoveForward(float Value);
    void MoveRight(float Value);
    void HandleSpellInput();

    // 環境傷害：站在 Fire/Lava tile 時每幀扣血
    void ApplyEnvironmentalDamage(float DeltaTime);

    UPROPERTY() TObjectPtr<AVoxelWorldActor> CachedVoxelWorld;
};
