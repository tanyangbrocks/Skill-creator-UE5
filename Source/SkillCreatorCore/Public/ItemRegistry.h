#pragma once
#include "CoreMinimal.h"
#include "ItemData.h"

// 物品定義靜態登錄表（對應 Godot ItemRegistry.cs static class）
// 首次呼叫 Get() 時初始化，執行緒安全（function-local static）
struct SKILLCREATORCORE_API FItemRegistry
{
    static const FItemData& Get(EItemId Id);

    // 2026-06-23：後置註冊 PlaceAsActor（泛型 TSubclassOf<AActor>，避免這個低層模組反過來
    // 依賴實際的 Actor 類別所在的高層模組）。由 SkillCreatorRuntime 模組 StartupModule()
    // 呼叫一次，把 AChestActor/AWorkbenchActor 等類別補上去。
    static void SetPlaceAsActor(EItemId Id, TSubclassOf<AActor> ActorClass);

private:
    static void Init(TArray<FItemData>& Out);
    static TArray<FItemData>& GetMutableTable();
};
