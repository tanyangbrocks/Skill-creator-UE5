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

    // Contact 類型：前方 MeleeRange 格 3D 掃描，命中後施加元素 Aura + 傷害 + SpawnEffect
    void ExecuteContactHit(const FSpellArray& Spell);

    virtual void BeginPlay() override;
};
