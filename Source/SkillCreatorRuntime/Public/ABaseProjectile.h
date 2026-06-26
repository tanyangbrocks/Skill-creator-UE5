#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridPos.h"
#include "ABaseProjectile.generated.h"

class AVoxelWorldActor;

// 離散 tile 移動投射物的共同基底類
// - 每 MoveInterval 秒推進一格，支援對角線/任意方向（浮點累積 + round）
// - 碰撞優先順序：① OnTileEntered（子類做生物/玩家命中偵測）→ ② OnHitSolidTile（實心 tile）
// - 子類在 OnTileEntered 命中時 Destroy()，基底偵測 !IsValid() 後略過 tile 碰撞
// 子類：ASpellProjectile（技能投射物）
UCLASS(Abstract)
class SKILLCREATORRUNTIME_API ABaseProjectile : public AActor
{
    GENERATED_BODY()
public:
    ABaseProjectile();

    UPROPERTY(EditAnywhere, Category="Projectile")
    float MoveInterval = 0.08f;   // 秒/格

    UPROPERTY(EditAnywhere, Category="Projectile")
    int32 MaxRange = 20;          // 最大飛行格數

    virtual void Tick(float DeltaTime) override;

protected:
    // 子類在自己的 Init() 最後呼叫此函式
    void InitBase(FGridPos InOrigin, FVector InDir, AVoxelWorldActor* InVoxelWorld);

    // 移動到新 tile 時呼叫（子類實作生物命中偵測；命中時呼叫 Destroy()）
    virtual void OnTileEntered(FGridPos NewTile) {}

    // 命中實心 tile 時呼叫（預設 Destroy()；子類可覆寫做 CA 爆炸等後處理）
    virtual void OnHitSolidTile(FGridPos TilePos) { Destroy(); }

    FGridPos CurrentPos;
    FVector  TileDir;      // 正規化浮點方向（GetSafeNormal() 結果）
    FVector  FloatPos;     // 累積浮點位置；整數部份決定目標 tile
    int32    TilesTravelled = 0;
    float    MoveTimer = 0.f;

    UPROPERTY() TObjectPtr<AVoxelWorldActor> VoxelWorld;

private:
    void AdvanceOneTile();
};
