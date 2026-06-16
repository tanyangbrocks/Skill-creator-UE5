#include "UGameFlowWidget.h"
#include "FlowSaveSystem.h"
#include "WorldSaveData.h"
#include "UGameSessionSubsystem.h"
#include "Kismet/GameplayStatics.h"

// ── 內部轉換 ──────────────────────────────────────────────────────────────

static FWorldSaveInfo ToInfo(const FWorldSaveData& D)
{
    FWorldSaveInfo I;
    I.Id       = D.Id;
    I.Name     = D.Name;
    I.Seed     = D.Seed;
    I.WorldDir = D.WorldDir;
    return I;
}

// ── UUserWidget overrides ─────────────────────────────────────────────────

void UGameFlowWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshWorldList();
}

// ── BlueprintCallable ─────────────────────────────────────────────────────

void UGameFlowWidget::RefreshWorldList()
{
    TArray<FWorldSaveData> All;
    FFlowSaveSystem::ListAllWorlds(All);

    CachedWorlds.Reset(All.Num());
    for (const FWorldSaveData& D : All)
        CachedWorlds.Add(ToInfo(D));

    OnWorldListRefreshed(CachedWorlds);
}

bool UGameFlowWidget::CreateWorld(const FString& Name, int32 Seed)
{
    FWorldSaveData NewWorld;
    if (!FFlowSaveSystem::CreateNewWorld(Name, Seed, NewWorld)) return false;
    RefreshWorldList();
    return true;
}

bool UGameFlowWidget::RemoveWorld(const FString& WorldId)
{
    if (!FFlowSaveSystem::DeleteWorld(WorldId)) return false;
    RefreshWorldList();
    return true;
}

void UGameFlowWidget::EnterWorld(const FString& WorldId)
{
    // 找到目標世界
    const FWorldSaveInfo* Found = CachedWorlds.FindByPredicate(
        [&WorldId](const FWorldSaveInfo& I){ return I.Id == WorldId; });
    if (!Found) return;

    // 載入完整 meta（含 PlayerSpawn 等欄位）
    FWorldSaveData Meta;
    if (!FFlowSaveSystem::LoadWorldMeta(Found->WorldDir, Meta)) return;
    Meta.WorldDir = Found->WorldDir;

    // 存入跨關卡持久子系統
    if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
    {
        if (UGameSessionSubsystem* Sub = GI->GetSubsystem<UGameSessionSubsystem>())
            Sub->SetPendingWorld(Meta);
    }

    OnEnterGame(*Found);
}
