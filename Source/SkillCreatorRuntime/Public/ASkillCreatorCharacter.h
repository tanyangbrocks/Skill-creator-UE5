#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"

struct FInputActionValue;
#include "ICreature.h"
#include "IElementalTarget.h"
#include "ISnapshottable.h"
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
class AEnemyManager;
struct FCharacterSaveData;

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
    // 包含：防禦/減傷 → 暴擊判定 → 命中/閃避判定 → TakeDirectDamage
    // AttackerStats 為 nullptr 時跳過暴擊/閃避判定（純防禦計算）
    void TakePhysicalDamage(float PhysAtk, const FCharacterStats* AttackerStats = nullptr);
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

    // 放置目標格是否被玩家/敵人佔據（對應 Main.cs:1654 OccupiedByEntity）
    bool IsTileOccupiedByEntity(const FIntVector& Pos) const;

    // ── 採掘狀態（對應 Godot PlayerController.cs MiningTarget/MiningProgress）──
    // 漸進式採掘：換目標重置進度；達到材質 Hardness 才真正摧毀中心格。
    TOptional<FIntVector> MiningTarget;
    float                 MiningProgress = 0.f;
    void CancelMining() { MiningTarget.Reset(); MiningProgress = 0.f; }

    // ── 放置節流狀態（對應 Godot Main.cs _placeCooldown/_rightWasPressed）──
    float PlaceCooldown          = 0.f;
    bool  bRightMouseWasPressed  = false;

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
};
