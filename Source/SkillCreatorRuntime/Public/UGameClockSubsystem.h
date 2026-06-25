#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UGameClockSubsystem.generated.h"

// ── 曆法快照（GetDateTime() 回傳）────────────────────────────────────────────
USTRUCT(BlueprintType)
struct FGameDateTime
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="GameClock") int32 Year      = 5000;
    UPROPERTY(BlueprintReadOnly, Category="GameClock") int32 Month     = 1;
    UPROPERTY(BlueprintReadOnly, Category="GameClock") int32 Day       = 1;
    UPROPERTY(BlueprintReadOnly, Category="GameClock") int32 Hour      = 0;
    UPROPERTY(BlueprintReadOnly, Category="GameClock") int32 Minute    = 0;
    UPROPERTY(BlueprintReadOnly, Category="GameClock") int32 DayOfWeek = 0; // 0=一,1=二…6=日
};

// ── 遊戲內時間系統 ────────────────────────────────────────────────────────────
// 時間比例（TotalTicks 為核心）：
//   TicksPerSecond = 20  →  1 遊戲分鐘 = 1 現實秒 = 20 ticks
//   TicksPerHour   = 1200 → 1 遊戲小時 = 60 現實秒
//   TicksPerDay    = 28800 → 1 遊戲日   = 1440 現實秒 = 24 現實分鐘
//
// 曆法：蒼和曆（起始：5000年 1月1日 00:00 星期一）
//   每月 30 天，每年 12 月（360天）；每週 7 天
//
// TotalTicks 同時作為 SpellRunner.SubmittedAt / Snapshot.AnchorTimestamp 的時間戳，
// 不可重命名；實際語意為「本世界累積 tick 數」（已從 WorldSaveData 載入）。
UCLASS()
class SKILLCREATORRUNTIME_API UGameClockSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:

    // ── Tick 常數 ──────────────────────────────────────────────────────────
    static constexpr int32 TicksPerSecond  = 20;
    static constexpr int32 TicksPerMinute  = 20;    // = TicksPerSecond（1遊戲分鐘=1現實秒）
    static constexpr int32 TicksPerHour    = 1200;  // 60 × 20
    static constexpr int32 TicksPerDay     = 28800; // 24 × 60 × 20

    // ── 曆法常數 ───────────────────────────────────────────────────────────
    static constexpr int32 StartYear       = 5000;
    static constexpr int32 DaysPerMonth    = 30;
    static constexpr int32 MonthsPerYear   = 12;
    static constexpr int32 DaysPerYear     = 360;   // 12 × 30

    // ── 晝夜邊界（自當天 00:00 起算的 tick 偏移）──────────────────────────
    // 白天：04:30 ~ 19:30
    static constexpr int32 DawnTickOfDay   = 5400;  // (4*60+30)*20
    static constexpr int32 DuskTickOfDay   = 23400; // (19*60+30)*20

    // ── 狀態 ───────────────────────────────────────────────────────────────
    // 本世界累積 tick 數（由 GameMode::StartGameplayWithWorld 從 WorldSaveData 載入）
    UPROPERTY(BlueprintReadOnly, Category="GameClock")
    int64 TotalTicks = 0;

    UPROPERTY(BlueprintReadOnly, Category="GameClock")
    bool bClockPaused = false;

    // ── 曆法查詢 ───────────────────────────────────────────────────────────
    UFUNCTION(BlueprintPure, Category="GameClock")
    FGameDateTime GetDateTime() const;

    // 格式：「蒼和XXXX年 XX月XX日 XX：XX 星期X」
    UFUNCTION(BlueprintPure, Category="GameClock")
    FString GetFormattedTime() const;

    // 是否為白天（04:30 ~ 19:30）
    UFUNCTION(BlueprintPure, Category="GameClock")
    bool IsDay() const;

    // 當天進度 0.0~1.0（視覺系統用，如太陽位置）
    UFUNCTION(BlueprintPure, Category="GameClock")
    float GetDayFraction() const
    {
        return (float)(TotalTicks % TicksPerDay) / (float)TicksPerDay;
    }

    // ── 控制 ───────────────────────────────────────────────────────────────
    // 載入存檔時由 GameMode 呼叫（新世界傳 0，舊世界傳 WorldSaveData.ElapsedTicks）
    void SetTicks(int64 Ticks);
    void PauseClock();
    void ResumeClock();

    // ASkillCreatorCharacter::Tick 每幀呼叫
    void Advance(float DeltaSeconds);

private:
    float Accumulator = 0.f;
};
