#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "ANPCCharacter.h"
#include "ANPCAIController.generated.h"

class FTileWorld3D;

// NPC AI 控制器：直接在 Tick() 裡執行優先順序 Flee > Follow > Hostile > Wander，
// 不需要 BT_WanderingBard.uasset。若日後設定 BehaviorTreeAsset，自動讓 BT 接管。
UCLASS()
class SKILLCREATORRUNTIME_API ANPCAIController : public AAIController
{
    GENERATED_BODY()
public:
    ANPCAIController();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    TObjectPtr<class UBehaviorTree> BehaviorTreeAsset;

    UPROPERTY(EditAnywhere, Category="AI") int32 WanderRadiusTiles  = 16;
    UPROPERTY(EditAnywhere, Category="AI") float MinRetryInterval   = 3.f;
    UPROPERTY(EditAnywhere, Category="AI") float MaxRetryInterval   = 8.f;
    UPROPERTY(EditAnywhere, Category="AI") int32 AttackRangeTiles   = 2;
    UPROPERTY(EditAnywhere, Category="AI") float MoveInterval       = 0.35f;

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void Tick(float DeltaTime)    override;

private:
    float MoveCooldown = 0.f;

    void StepFlee         (ANPCCharacter* NPC, FTileWorld3D* TW);
    void StepFollow       (ANPCCharacter* NPC, FTileWorld3D* TW);
    void StepCounterAttack(ANPCCharacter* NPC, FTileWorld3D* TW);
    void StepWander       (ANPCCharacter* NPC, FTileWorld3D* TW);

    // 共用：往 (Dx, Dz) 方向嘗試移動 NPC 一步，返回是否成功
    bool TryStep(ANPCCharacter* NPC, FTileWorld3D* TW, int32 Dx, int32 Dz);
};
