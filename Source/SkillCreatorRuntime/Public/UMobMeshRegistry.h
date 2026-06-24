#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UMobMeshRegistry.generated.h"

class USkeletalMesh;
class UMaterialInterface;
class UAnimInstance;

// 生物模型登記表（docs/plan-base-npc-system.md §四「資產管線」，落地
// docs/plan-asset-registry.md §5-1 原本擱置的設計）。FName key（RaceId 或 SubtypeId 皆可），
// 不限於 NPC——AEnemy/玩家角色之後要接模型也能用同一張表，不必另外設計。
//
// 2026-06-23 教訓（見本次稽核 TileMaterialRegistry 真根因修復）：TSoftObjectPtr::Get() 不會
// 觸發載入，只有「已經被別處載入」才回傳非 null；這裡從一開始就用 LoadSynchronous()，
// 不要重蹈覆轍。
USTRUCT(BlueprintType)
struct FMobMeshEntry : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<USkeletalMesh>      Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UMaterialInterface> Material;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftClassPtr<UAnimInstance>       AnimBPClass;
};

// 已解析（LoadSynchronous 過）的查詢結果
struct FMobMeshResolved
{
    USkeletalMesh*               Mesh        = nullptr;
    UMaterialInterface*          Material    = nullptr;
    TSubclassOf<UAnimInstance>   AnimBPClass = nullptr;
    bool IsValid() const { return Mesh != nullptr; }
};

struct SKILLCREATORRUNTIME_API FMobMeshRegistry
{
    // Key 找不到、或 DataTable 資產（/Game/Data/DT_MobMeshRegistry）還沒建立時，
    // 回傳 IsValid()==false——呼叫端（ANPCCharacter 等）負責 fallback 到預設骨架網格，
    // 跟 AVoxelWorldActor「Registry 找不到 fallback 到 VoxelMaterial」同一個容錯模式。
    static FMobMeshResolved Find(FName Key);

private:
    static UDataTable* GetTable();
};
