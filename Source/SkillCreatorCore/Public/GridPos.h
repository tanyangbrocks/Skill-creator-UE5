#pragma once
#include "CoreMinimal.h"
#include "GridPos.generated.h"

// 3D 格座標包裝型別（對應 Godot Vector3I 邏輯層用途）
// 不繼承 UObject；直接作 TMap key，需手動提供 GetTypeHash（見下方）
USTRUCT(BlueprintType)
struct FGridPos
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 X = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Y = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Z = 0;

    FGridPos() = default;
    FGridPos(int32 InX, int32 InY, int32 InZ) : X(InX), Y(InY), Z(InZ) {}

    bool operator==(const FGridPos& O) const { return X == O.X && Y == O.Y && Z == O.Z; }
    bool operator!=(const FGridPos& O) const { return !(*this == O); }

    FGridPos operator+(const FGridPos& O) const { return { X + O.X, Y + O.Y, Z + O.Z }; }
    FGridPos operator-(const FGridPos& O) const { return { X - O.X, Y - O.Y, Z - O.Z }; }

    int32 ManhattanDistance(const FGridPos& O) const
    {
        return FMath::Abs(X - O.X) + FMath::Abs(Y - O.Y) + FMath::Abs(Z - O.Z);
    }

    // Chebyshev（棋盤格）距離：適用於 tile-based 攻擊範圍判斷（對角線視為 1 格）
    int32 ChebyshevDistance(const FGridPos& O) const
    {
        return FMath::Max3(FMath::Abs(X - O.X), FMath::Abs(Y - O.Y), FMath::Abs(Z - O.Z));
    }

    FString ToString() const { return FString::Printf(TEXT("(%d,%d,%d)"), X, Y, Z); }
};

// TMap<FGridPos, ...> 需要此函式；FIntVector 在 UE5 已有內建 hash 可參考
FORCEINLINE uint32 GetTypeHash(const FGridPos& Pos)
{
    uint32 Hash = GetTypeHash(Pos.X);
    Hash = HashCombine(Hash, GetTypeHash(Pos.Y));
    return HashCombine(Hash, GetTypeHash(Pos.Z));
}
