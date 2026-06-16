#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UGameClockSubsystem.generated.h"

// 遊戲內時間系統（對應 Godot GameClock.cs）
// 1 現實秒 = 20 ticks；1 遊戲日 = 28,800 ticks（24 現實分鐘）
// 由 ASkillCreatorCharacter::Tick 驅動（每幀 Advance）
UCLASS()
class SKILLCREATORRUNTIME_API UGameClockSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    static constexpr int32 TicksPerSecond = 20;
    static constexpr int32 TicksPerDay    = 28800; // 24 real minutes

    // 自本局開始累積的總 tick 數
    UPROPERTY(BlueprintReadOnly, Category="GameClock")
    int64 TotalTicks = 0;

    // 已過天數（0-based）
    UPROPERTY(BlueprintReadOnly, Category="GameClock")
    int32 DayCount = 0;

    // 當天進度（0.0 = 日出，0.5 = 正午，1.0 = 隔天日出）
    UFUNCTION(BlueprintPure, Category="GameClock")
    float GetDayFraction() const
    {
        return (float)(TotalTicks % TicksPerDay) / (float)TicksPerDay;
    }

    // 換算成 0-24 小時制（0.0 = 0:00，12.0 = 正午）
    UFUNCTION(BlueprintPure, Category="GameClock")
    float GetDayHour() const { return GetDayFraction() * 24.f; }

    // ASkillCreatorCharacter::Tick 每幀呼叫
    void Advance(float DeltaSeconds);

    // 場景重啟時呼叫
    void Reset();

private:
    float Accumulator = 0.f;
};
