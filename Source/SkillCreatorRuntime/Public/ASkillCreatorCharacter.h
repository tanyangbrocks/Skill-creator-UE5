#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AWeedEntity.h"

struct FInputActionValue;
#include "ICreature.h"
#include "IElementalTarget.h"
#include "ISnapshottable.h"
#include "CharacterStats.h"
#include "ActionBus.h"
#include "SkillCameraTypes.h"
#include "ManaSlot.h"
#include "AttackTypes.h"
#include "ASkillCreatorCharacter.generated.h"

// S-1：玩家移動狀態機
UENUM(BlueprintType)
enum class EPlayerMovementState : uint8
{
    Grounded       UMETA(DisplayName="地面"),
    Sprinting      UMETA(DisplayName="疾跑"),
    SuperSprinting UMETA(DisplayName="超速"),
    Flying         UMETA(DisplayName="飛行"),
    FastFlying     UMETA(DisplayName="疾飛"),
    Guarding       UMETA(DisplayName="防禦"),
    Crouching      UMETA(DisplayName="蹲"),
    Rolling        UMETA(DisplayName="翻滾"),
    Sliding        UMETA(DisplayName="滑鏟"),
};

class UCharacterStateComponent;
class UElementalAuraComponent;
class USpellCaster;
class USpringArmComponent;
class UCameraComponent;
class UInventoryComponent;
class UEquipmentComponent;
class AVoxelWorldActor;
class AEnemyManager;
class AEnemy;
struct FCharacterSaveData;
class UPotionBagComponent;
class UMapComponent;
class UAfterimageFXComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHpChanged, float, NewHp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterDied);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnParrySuccess, AActor*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAttackHit, EAttackType, AActor*);

// 玩家角色（對應 Godot PlayerController.cs 角色層）。
// EntityId 固定 -1；HP/MP 從 Stats 初始化。
// 施法（SpellCaster）M-5；庫存 → M-7；生存系統 → M-7。
UCLASS()
class SKILLCREATORRUNTIME_API ASkillCreatorCharacter
    : public ACharacter
    , public ICreature
    , public IElementalTarget
    , public ISnapshottable
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

    // W-10: 等級系統
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Level")
    int32 Level = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Level")
    float Xp = 0.f;

    // 升級所需 XP（等級 × 100，對應 Godot XpRequired）
    static int32 XpRequired(int32 InLevel) { return InLevel * 100; }

    // 當前境界名稱（對應 Godot GetTierName）
    static FString GetTierName(int32 InLevel);

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

    // ── S-1 移動狀態機 ────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Movement")
    EPlayerMovementState MovementState = EPlayerMovementState::Grounded;

    // Z 按住：疾跑（2x）；長按 1s → 超速（4x）；飛行中：切換疾飛
    void StartSprint();
    void EndSprint();
    // K（空中）：進入/退出飛行（MOVE_Flying，重力=0）
    void ToggleFlight();
    // X（飛行中 or 空中）：取消飛行 / 快速墜落；地面：蹲/翻滾/滑鏟
    void FlyDown();
    void StartFastFall();
    void PerformCrouch();
    void EndCrouch();
    void PerformRoll();
    void PerformSlide();
    void EndSlide();

    bool IsFlying() const
    {
        return MovementState == EPlayerMovementState::Flying
            || MovementState == EPlayerMovementState::FastFlying;
    }
    bool IsCrouching()      const { return MovementState == EPlayerMovementState::Crouching; }
    bool IsRolling()        const { return MovementState == EPlayerMovementState::Rolling; }
    bool IsSliding()        const { return MovementState == EPlayerMovementState::Sliding; }
    bool IsSuperSprinting() const { return MovementState == EPlayerMovementState::SuperSprinting; }

    // ── S-3 鎖敵 ──────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    TObjectPtr<AEnemy> LockedTarget;

    // E：切換鎖定（已鎖→解鎖；未鎖→找最近屏幕中心目標）
    bool TryToggleLockTarget();
    // Tab：循環切換至下一個有效目標
    void SwitchToNextLockTarget();
    bool IsLockingTarget() const { return LockedTarget != nullptr; }

    // ── S-4 防禦/彈反 ────────────────────────────────────────────
    // X 長按（地面）：進入 Guarding 狀態；X+J（6 幀窗口）＝彈反
    // 彈反成功：完全無傷 + 凍結近戰攻擊者 3 秒（AuraComp->ApplyFreeze）
    // 防禦中但彈反窗口已過：承受 50% 物理傷害
    void PerformGuard();
    void EndGuard();
    bool IsGuarding() const { return MovementState == EPlayerMovementState::Guarding; }

    // S-8 接口：L 鍵後撤衝量 + 殘影 FX（AfterimageComp stub）
    void PerformBackDash();

    // 彈反成功事件（HUD / 音效 / VFX 接收；AActor* = 被彈反的攻擊者）
    FOnParrySuccess OnParrySuccess;

    // ── S-2 完整攻擊框架 ──────────────────────────────────────────
    // J IE_Pressed → StartChargingAttack；IE_Released → ReleaseAttack（輕攻 or 蓄力攻）
    // J+U（J 已蓄力 ≥0.3s）→ PerformHeavyAttack；H / 被打斷 → CancelAttack
    void StartChargingAttack();
    void ReleaseAttack();
    void PerformHeavyAttack();
    void CancelAttack();
    bool  IsChargingAttack() const { return bChargingAttack; }
    float GetChargeTime()    const { return AttackChargeTimer; }

    // 攻擊命中事件（HUD/VFX/拼刀系統接收）
    FOnAttackHit OnAttackHit;

    // 保留供 SpellCaster Contact 觸發呼叫
    void PerformLightAttack();

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

    // S-6 接口（stub — 藥水袋系統完成後子類覆寫）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UPotionBagComponent> PotionBagComp;

    // S-7 接口（stub — 地圖系統完成後子類覆寫）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UMapComponent> MapComp;

    // S-8 接口（stub — 殘影 FX 系統完成後子類覆寫）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UAfterimageFXComponent> AfterimageComp;

    // ── 事件 ──────────────────────────────────────────────────────
    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnHpChanged OnHpChanged;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnCharacterDied OnCharacterDied;

    // ── ActionBus（DamageShield / DeathGuard 攔截，DeltaTime 計時，pause-aware）──
    FActionBus ActionBus;

    // ── 戰鬥 API ──────────────────────────────────────────────────
    // UI-2: 讓 InputSettingsWidget 取得 IMC 進行鍵位重綁
    UFUNCTION(BlueprintPure, Category="Input")
    class UInputMappingContext* GetDefaultMappingContext() const { return DefaultIMC; }

    UFUNCTION(BlueprintCallable, Category="Combat")
    void GainXp(float Amount);

    UFUNCTION(BlueprintCallable, Category="Combat")
    bool IsAliveChar() const { return IsAlive(); }

    // 重新尋找並綁定 AVoxelWorldActor / AEnemyManager（GameMode 在 GameFlow 選好世界、
    // 延後生成 AVoxelWorldActor 之後呼叫；BeginPlay 當下世界可能還不存在）
    UFUNCTION(BlueprintCallable, Category="World")
    void RebindWorldSystems();

    // 套用玩家在 UGameFlowWidget 選定角色的存檔進度，取代 BeginPlay 當下
    // 灌入的預設數值（GameMode::StartGameplayWithWorld 選好角色後呼叫）。
    // 非 UFUNCTION：FCharacterSaveData 未標記 BlueprintType，且目前只有
    // C++ 呼叫端，不需要 Blueprint 曝露。
    void ApplyCharacterSaveData(const FCharacterSaveData& Data);

    // 把目前執行期狀態（Hp/Mp/Level/Xp/Position/Inventory/State/ManaCurrents）
    // 寫回 OutData。OutData 的 Id/CharacterName 由呼叫方事先填好（這裡不改動）。
    void FillSaveData(FCharacterSaveData& OutData) const;

    // ── ICreature ─────────────────────────────────────────────────
    virtual int32    GetCreatureId() const override { return -1; }
    virtual FGridPos GetPosition()   const override;
    virtual float    GetHp()         const override { return CurrentHp; }
    virtual float    GetMaxHp()      const override { return Stats.MaxHpBase; }
    virtual bool     IsAlive()       const override { return CurrentHp > 0.f; }

    // ── 傷害管線（B-3：帶完整 stats 的物理/能量傷害）─────────────
    // 包含：S-4 彈反/防禦判定 → 防禦/減傷 → 暴擊判定 → 命中/閃避判定 → TakeDirectDamage
    // AttackerStats 為 nullptr 時跳過暴擊/閃避判定（純防禦計算）
    // Attacker 為非 nullptr 時，彈反成功可凍結攻擊者（AEnemy 有 AuraComp）
    void TakePhysicalDamage(float PhysAtk, const FCharacterStats* AttackerStats = nullptr, AActor* Attacker = nullptr);
    void TakeEnergyDamage(float EnergyAtk, FName ManaTypeKey, const FCharacterStats* AttackerStats = nullptr);

    // ── IElementalTarget ──────────────────────────────────────────
    virtual int32 GetEntityId()              const override { return -1; }
    virtual void  TakeDirectDamage(float Amount)   override;

    // ── ISnapshottable ────────────────────────────────────────────
    virtual FEntitySnapshot TakeSnapshot()                              const override;
    virtual void            RestoreFromSnapshot(const FEntitySnapshot&)       override;

    // ── UE5 overrides ─────────────────────────────────────────────
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual void Landed(const FHitResult& Hit) override;

private:
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void MoveForward(float Value);
    void MoveRight(float Value);
    void HandleSpellInput();

    // 環境傷害：站在 Fire/Lava tile 時每幀扣血
    void ApplyEnvironmentalDamage(float DeltaTime);

    // B-4: HP/MP 0.5s regen 計時器（對應設計文件「每 0.5 秒恢復一次」）
    void TickRegen();
    FTimerHandle RegenTimerHandle;

    // S-4 防禦/彈反計時器（6 幀 @ 60fps ≈ 100ms）
    static constexpr float ParryWindowSec = 6.f / 60.f;
    bool bInParryWindow = false;
    FTimerHandle ParryWindowTimer;

    // S-2 攻擊蓄力狀態
    bool  bChargingAttack   = false;
    float AttackChargeTimer = 0.f;
    static constexpr float LightAttackMaxCharge = 0.5f;   // < 此秒數 = 輕攻
    static constexpr float HeavyAttackMinCharge = 0.3f;   // J+U 重攻最小蓄力（待平衡）
    static constexpr float MaxChargeTime        = 2.0f;   // 蓄力上限（待平衡）

    // S-1 移動擴展
    static constexpr int32 MaxJumpCount = 2;
    int32 JumpCount = 0;
    EPlayerMovementState PreActionState = EPlayerMovementState::Grounded;
    FTimerHandle RollTimer;
    FTimerHandle SprintTimer;

    // ── 面板 / 偵錯按鍵處理 ──────────────────────────────────────
    // 2026-06-19 稽核：原本這裡還有 OnOpenInventory/OnOpenEquipment/OnOpenCharacterPanel
    // 透過 Enhanced Input 把 Z/X/C 也綁了一份，但只會印 GEngine 偵錯文字，跟
    // ASkillCreatorPlayerController 用 legacy BindKey 綁的同一批鍵（呼叫 HUD 真正開關
    // 面板）同時觸發——玩家按一次鍵同時看到面板開關又被洗一堆偵錯文字。已刪除這份重複
    // 綁定，Z/X/C 的真正行為統一由 PlayerController 端負責。
    void OnToggleCameraMode();
    void OnDebugTrace();
    void OnDebugSnapshotTake();
    void OnDebugSnapshotApply();
    void OnMine();
    void OnPlace();
    void OnMineReleased();
    void OnPlaceReleased();

    // 左鍵 Tap/Hold 雙軸分派（docs/plan-item-crafting-system.md §衝突3）：
    // 短按＝OnPrimaryTap（攻擊/道具），長按＝沿用 OnMine（採礦/道具）。
    // bIsConsumable 物品覆寫兩者，都變成使用道具；長按時用 rising-edge 防止每幀重複使用。
    void OnPrimaryTap();
    bool bMineWasPressed = false;
    void UseConsumable();

    // 放置目標格是否被玩家/敵人佔據（對應 Main.cs:1654 OccupiedByEntity）
    bool IsTileOccupiedByEntity(const FIntVector& Pos) const;

    // ── 採掘狀態（對應 Godot PlayerController.cs MiningTarget/MiningProgress）──
    // 漸進式採掘：換目標重置進度；達到材質 Hardness 才真正摧毀中心格。
    TOptional<FIntVector> MiningTarget;
    float                 MiningProgress = 0.f;
    void CancelMining() { MiningTarget.Reset(); MiningProgress = 0.f; }

    // ── W-D：採集狀態（Entity 採集，固定 0.5s）────────────────────────────
    // 不同於採掘：對象是世界中的可採集 Actor（AWeedEntity 等），無材質硬度，固定時間。
    static constexpr float CollectFixedTime = 0.5f;
    TWeakObjectPtr<AActor> CollectTarget;
    float                  CollectProgress = 0.f;
    void CancelCollect() { CollectTarget.Reset(); CollectProgress = 0.f; }

    // ── W-G：雜草 random tick（MC 機制，每 GameTick 對 loaded chunk 取樣）──
    // 避免依賴 AVoxelWorldActor → AWeedEntity（循環依賴 VoxelWorld→Runtime），
    // 改放在 Runtime 層的角色 Tick 裡統一執行。
    void TickWeedGrowth(float DeltaTime);

    UFUNCTION()
    void OnWeedDestroyed(AActor* DestroyedActor);

    float WeedTickAccum = 0.f;
    TSet<FIntVector> ActiveWeedTiles;  // 已存在雜草的 tile，防重複

    // ── 放置節流狀態（對應 Godot Main.cs _placeCooldown/_rightWasPressed）──
    float PlaceCooldown          = 0.f;
    bool  bRightMouseWasPressed  = false;

    // Space 長按大跳（plan-player-actions.md §B）
    void OnJumpStarted();
    void OnJumpReleased();
    float JumpPressedTime = -1.f;

    // S-1：根據 MovementState 設定對應速度
    void ApplyMovementState();

    // F1/F2/F4 開發者工具（對應 Godot TogglePaint/DebugCoord/DebugSurvival）
    bool bDebugPaintEnabled    = false;
    bool bDebugCoordEnabled    = false;
    bool bDebugSurvivalEnabled = false;
    void OnDebugPaint();
    void OnDebugCoord();
    void OnDebugSurvival();

    UPROPERTY() TObjectPtr<AVoxelWorldActor>    CachedVoxelWorld;
    UPROPERTY() TObjectPtr<AEnemyManager>       CachedEnemyMgr;

    // UI-2: 供 UInputSettingsWidget 讀取以實現鍵位重綁
    UPROPERTY() TObjectPtr<class UInputMappingContext> DefaultIMC;

    // 互動系統（docs/plan-item-crafting-system.md §五）：每幀 Tick 更新，OnPlace() 互動優先判定使用
    UPROPERTY() TWeakObjectPtr<AActor> CachedInteractable;
    void UpdateInteractableTrace();

    // 採掘高亮（2026-06-23 修正）：算出這次按下去真正會被摧毀的整個 tile 集合（中心格
    // 放在 [0]），對應 Godot Main.cs:2935 ComputeHighlightTiles()。空陣列＝不顯示高亮
    // （不可挖/工具等級不足/超出採礦範圍/滑鼠在熱鍵欄上時皆回傳空，原版 UE5 完全沒做
    // 這些過濾，只要 raycast 打到東西就一律顯示）。
    TArray<FGridPos> ComputeHighlightTiles() const;
};
