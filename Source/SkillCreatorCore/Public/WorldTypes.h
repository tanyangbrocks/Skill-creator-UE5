#pragma once
#include "CoreMinimal.h"
#include "WorldTypes.generated.h"

// 方塊摧毀原因（對應 Godot DestroyReason.cs）
// 影響掉落物與特效；由 IWorldInterface::DestroyTile 傳入
UENUM(BlueprintType)
enum class EDestroyReason : uint8
{
    Mining      UMETA(DisplayName="採礦"),       // 一般採礦工具
    ShapeMining UMETA(DisplayName="形狀採礦"),   // 範圍採礦（N-1 面內全部破壞）
    Explosion   UMETA(DisplayName="爆炸"),
    Slash       UMETA(DisplayName="斬擊"),
    Crush       UMETA(DisplayName="碾壓"),
};
