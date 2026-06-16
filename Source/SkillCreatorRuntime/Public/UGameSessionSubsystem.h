#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WorldSaveData.h"
#include "CharacterSaveData.h"
#include "UGameSessionSubsystem.generated.h"

// 跨關卡持久：保存「玩家選擇進入哪個世界」
// MenuLevel 的 UGameFlowWidget 寫入；GameLevel 的 ASkillCreatorGameMode 讀取
UCLASS()
class SKILLCREATORRUNTIME_API UGameSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    // ── GameFlowUI 側寫入 ─────────────────────────────────────────────────
    void SetPendingWorld(const FWorldSaveData& World) { PendingWorld = World; bHasPending = true; }
    void ClearPending()                               { bHasPending = false; }

    // ── GameMode 側讀取 ──────────────────────────────────────────────────
    bool           HasPendingWorld()  const { return bHasPending;   }
    FWorldSaveData GetPendingWorld()  const { return PendingWorld;  }

private:
    FWorldSaveData PendingWorld;
    bool           bHasPending = false;
};
