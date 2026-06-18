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
    void SetPendingWorld(const FWorldSaveData& World) { PendingWorld = World; bHasPendingWorld = true; }
    void SetPendingCharacter(const FCharacterSaveData& Character) { PendingCharacter = Character; bHasPendingCharacter = true; }
    void ClearPending() { bHasPendingWorld = false; bHasPendingCharacter = false; }

    // ── GameMode 側讀取 ──────────────────────────────────────────────────
    bool               HasPendingWorld()     const { return bHasPendingWorld;     }
    FWorldSaveData     GetPendingWorld()     const { return PendingWorld;         }
    bool               HasPendingCharacter() const { return bHasPendingCharacter; }
    FCharacterSaveData GetPendingCharacter() const { return PendingCharacter;     }

private:
    FWorldSaveData     PendingWorld;
    bool               bHasPendingWorld = false;
    FCharacterSaveData PendingCharacter;
    bool               bHasPendingCharacter = false;
};
