#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpellArray.h"
#include "SpellGroup.h"
#include "Instruction.h"
#include "SpellRunner.h"
#include "USpellCaster.generated.h"

class ASkillCreatorCharacter;
class AEnemyManager;
class AVoxelWorldActor;

// UActorComponent 擁有 FSpellRunner，注入所有世界層 delegate，處理施法輸入。
// 掛載於 ASkillCreatorCharacter。
// M-5：接通 SpellRunner ↔ TileWorld3D + EnemyManager；Projectile 類型啟動 ASpellProjectile。
UCLASS(ClassGroup=SkillCreator, meta=(BlueprintSpawnableComponent))
class SKILLCREATORRUNTIME_API USpellCaster : public UActorComponent
{
    GENERATED_BODY()
public:
    USpellCaster();

    // 嘗試施放技能。Spell = 技能整構 metadata，Code = SpellCompiler 輸出。
    // 回傳 false 表示 MP 不足或仍在冷卻中。
    bool TryCast(const FSpellArray& Spell, const TArray<FInstruction>& Code);

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, Category="SpellCaster")
    float GlobalCooldown = 0.3f;

    // ── 技能組（5 組 × 10 欄，V 鍵切組）────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SpellCaster|Groups")
    FSpellGroup SpellGroups;

    // 切換目前配置中的主動欄位（clamped to 0~MaxSlots-1）
    UFUNCTION(BlueprintCallable, Category="SpellCaster|Groups")
    void SwitchSlot(int32 Idx);

    // 循環移動欄位選擇（wraps around MaxSlots）
    UFUNCTION(BlueprintCallable, Category="SpellCaster|Groups")
    void CycleSlot(int32 Delta);

    // 選中 SlotIndex 並嘗試施放（M-9 接 SpellCompiler；M-5 空 Code）
    void TryCastSlot(int32 SlotIndex);

private:
    FSpellRunner Runner;
    float        CooldownRemaining = 0.f;

    UPROPERTY() TObjectPtr<AEnemyManager>    CachedEnemyMgr;
    UPROPERTY() TObjectPtr<AVoxelWorldActor> CachedVoxelWorld;

    ASkillCreatorCharacter* GetOwnerCharacter() const;

    // 近戰接觸範圍（格數，對應 Godot SpellCaster.cs MeleeRange 常數）
    static constexpr int32 MeleeRange = 3;

    // Contact 類型：前方 MeleeRange 格 3D 掃描，命中後施加元素 Aura + 傷害 + SpawnEffect
    void ExecuteContactHit(const FSpellArray& Spell);

    virtual void BeginPlay() override;

    // ── 技能施放核心（技能因子派發，對應 Godot SpellCaster.cs）────────
    struct FSpellMods
    {
        float DmgBonus  = 0.f;
        int32 Multi     = 1;
        bool  bFire = false, bWater = false, bIce = false, bThunder = false;
        float PushDist  = 0.f, PullDist  = 0.f;
        float SlowDur   = 0.f, FreezeDur = 0.f, StunDur = 0.f;
        float HpCost    = 0.f;
    };

    static FSpellMods ReadMods(const FSpellSlot& Slot);
    static void       SpawnEffectTiles(AVoxelWorldActor* VW, FName EffectType,
                                       FGridPos Pos, int32 Radius);

    void ApplyGlobalEngravings(const FSpellArray& Spell);
    void ApplyModsToNearbyEnemies(const FSpellMods& Mods, FGridPos Origin,
                                   AVoxelWorldActor* VW);
    void ResolveTotem(FName TotemName, const FSpellSlot& Slot,
                      FExecutionContext& Ctx, bool bAtHitPoint, AVoxelWorldActor* VW);
    void DispatchAction(const FString& ActionId, const FSpellSlot& Slot,
                        FGridPos Origin, bool bAtHitPoint, AVoxelWorldActor* VW);
    void ExecuteArea(const FString& Shape, const FSpellSlot& Slot,
                     FGridPos Origin, bool bFromHitPoint, AVoxelWorldActor* VW);
    void ExecuteTechnique(const FSpellSlot& Slot, FGridPos Origin,
                          bool bAtHitPoint, AVoxelWorldActor* VW);
    void ExecuteProjectileTotem(const FSpellSlot& Slot, FGridPos Origin,
                                AVoxelWorldActor* VW);
    void ExecuteMorph(const FSpellSlot& Slot, FGridPos Origin, AVoxelWorldActor* VW);
    void ExecuteDisplacement(const FString& ActionId, const FSpellSlot& Slot,
                              AVoxelWorldActor* VW);
    void ExecuteSummon(const FSpellSlot& Slot, FGridPos Origin, AVoxelWorldActor* VW);
    void ExecuteDomain(const FSpellSlot& Slot, FGridPos Origin, AVoxelWorldActor* VW);

    // 從 Spell + 已編譯 Code 建立完整 FExecutionContext（注入所有世界層 delegate）
    // TryCast 和 OnBuildComboContext 共用
    TUniquePtr<FExecutionContext> BuildContext(const FSpellArray& Spell,
                                               const TArray<FInstruction>& Code);
};
