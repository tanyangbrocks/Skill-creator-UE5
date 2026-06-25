#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ICreature.h"
#include "IElementalTarget.h"
#include "ISnapshottable.h"
#include "GridPos.h"
#include "SnapshotTypes.h"
#include "CharacterStats.h"
#include "ActionBus.h"
#include "NPCIdentity.h"
#include "NPCActionTypes.h"
#include "CreatureTypes.h"
#include "ANPCCharacter.generated.h"

class USkeletalMeshComponent;
class UNPCMemoryComponent;
class UNPCPerceptionComponent;
class UElementalAuraComponent;
class AVoxelWorldActor;
class AEnemyManager;
class FWorldInterfaceAdapter;

// NPC 的「身體」（docs/plan-base-npc-system.md §四）。參考 AEnemy 的既有模式但不繼承它——
// 語義上 NPC 不是敵人，獨立類別比較不會把「永遠敵對」的假設帶進來（沿用本專案「三行重複
// 好過早抽象」的慣例，跟 ASkillCreatorCharacter/AEnemy 各自獨立實作 TakePhysicalDamage 同一個道理）。
UENUM(BlueprintType)
enum class ENPCDisposition : uint8
{
    Neutral,   // 預設：不主動攻擊玩家
    Hostile,   // 被玩家攻擊後轉換；持續到 HostileCooldown 計時結束或死亡
};

UCLASS()
class SKILLCREATORRUNTIME_API ANPCCharacter
    : public APawn
    , public ICreature
    , public IElementalTarget
    , public ISnapshottable
{
    GENERATED_BODY()
public:
    ANPCCharacter();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC")
    FNPCIdentity Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC")
    FCharacterStats Stats;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC")
    ENPCDisposition Disposition = ENPCDisposition::Neutral;

    // 暫定 15 秒，純占位數值待平衡（docs/plan-base-npc-system.md §九 開放問題 Q2）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC")
    float HostileCooldownDuration = 15.f;
    float HostileCooldownRemaining = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC")
    FGridPos GridPosition;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC")
    FGridPos SpawnGridPos;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC")
    float Hp = 50.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC")
    float MaxHp = 50.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<USkeletalMeshComponent> MeshComp;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UNPCMemoryComponent> MemoryComp;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UNPCPerceptionComponent> PerceptionComp;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UElementalAuraComponent> AuraComp;

    UPROPERTY() TObjectPtr<AVoxelWorldActor> CachedVoxelWorld;
    UPROPERTY() TObjectPtr<AEnemyManager>    CachedEnemyMgr;

    // 隨機遊蕩狀態（docs/plan-base-npc-system.md §五，UBTTask_WanderRandomly 讀寫）。
    // 放在 Pawn 本身而不是 BT NodeMemory，跟 AEnemy::PatrolDir 同一個慣例。
    UPROPERTY() FGridPos WanderTarget;
    UPROPERTY() bool     bHasWanderTarget = false;
    UPROPERTY() float    WanderRetryTimer = 0.f;

    // M-NPC-4: last LLM inference result (M-NPC-5 dialogue UI reads these).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC|Brain")
    FString LastDialogue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC|Brain")
    FString LastEmotion;

    // M-NPC-6: action flags read by BT Tasks (UBTTask_NPCFlee / UBTTask_NPCFollow).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC|Brain")
    bool bFleeRequested = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC|Brain")
    TObjectPtr<AActor> FollowTarget = nullptr;

    // M-NPC-4: trigger a full LLM round-trip. PlayerInput may be empty (proximity / observation).
    // Result is delivered asynchronously; HandleBrainResponse fires on game thread.
    void TriggerDialogue(const FString& PlayerInput = TEXT(""));

    // SubtypeId 由 ANPCSpawnController 在 Spawn 後立刻呼叫；觸發 LoadOrGenerate（已有存檔
    // 直接讀回，沒有則生成）+ 依 Identity.RaceId 查 UMobMeshRegistry 套用模型
    void InitializeIdentity(FName InNPCId, FName SubtypeId);

    // 傷害攔截管線（與 AEnemy::ActionBus 同一套，讓傷害護盾類效果也能保護 NPC）
    FActionBus ActionBus;

    // B-3 管線（命中/閃避/防禦/暴擊）；額外行為：傷害 > 0 → Disposition=Hostile
    void TakePhysicalDamage(float PhysAtk, const FCharacterStats* AttackerStats = nullptr);
    void TakeElementalDamage(float ElemAtk, ESkillElementType Element, bool bEnergyDefenseApplies = false, const FCharacterStats* AttackerStats = nullptr);
    void TakeDamageAmount(float Amount);

    virtual void Tick(float DeltaTime) override;

    // ── 生物分類（plan-creature-redesign）────────────────────────
    ECreatureKind GetCreatureKind() const { return ECreatureKind::NPC; }
    bool          IsEnemy()        const { return Disposition == ENPCDisposition::Hostile; }

    // ── ICreature ─────────────────────────────────────────────────
    virtual int32    GetCreatureId() const override { return UniqueId; }
    virtual FGridPos GetPosition()   const override { return GridPosition; }
    virtual float    GetHp()         const override { return Hp; }
    virtual float    GetMaxHp()      const override { return MaxHp; }
    virtual bool     IsAlive()       const override { return Hp > 0.f; }

    // ── IElementalTarget ──────────────────────────────────────────
    virtual int32 GetEntityId()            const override { return UniqueId; }
    virtual void  TakeDirectDamage(float Amount) override { TakeDamageAmount(Amount); }

    // ── ISnapshottable ────────────────────────────────────────────
    virtual FEntitySnapshot TakeSnapshot()                              const override;
    virtual void            RestoreFromSnapshot(const FEntitySnapshot&)       override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
    int32        UniqueId = 0;
    static int32 NextId;

    // FWorldInterfaceAdapter 是純 C++ class（非 UObject），ANPCCharacter 持有它的生命週期。
    // 故意用裸 pointer 不用 TUniquePtr：UHT 幫 UCLASS 產生的 FVTableHelper 建構子
    // （ANPCCharacter.gen.cpp 的 DEFINE_VTABLE_PTR_HELPER_CTOR_NS）在不包含
    // FWorldInterfaceAdapter.h 的那個 TU 裡也會嘗試實體化 TUniquePtr 的解構子，
    // 對不完整型別失敗——裸 pointer 的隱式解構子是 trivial 的，不需要完整型別，
    // 在 BeginPlay/EndPlay 手動 new/delete 即可避開這個問題。
    FWorldInterfaceAdapter* WorldAdapter = nullptr;

    void ApplyMeshFromRegistry();

    // M-NPC-4: called on game thread after inference; writes memory note + dispatches action.
    void HandleBrainResponse(const FNPCBrainResponse& Response);
    // M-NPC-6: maps ENPCAction to concrete game behavior.
    void DispatchBrainAction(ENPCAction Action);
};
