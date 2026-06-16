#pragma once
#include "CoreMinimal.h"
#include "ItemData.h"

// 物品定義靜態登錄表（對應 Godot ItemRegistry.cs static class）
// 首次呼叫 Get() 時初始化，執行緒安全（function-local static）
struct SKILLCREATORCORE_API FItemRegistry
{
    static const FItemData& Get(EItemId Id);
private:
    static void Init(TArray<FItemData>& Out);
};
