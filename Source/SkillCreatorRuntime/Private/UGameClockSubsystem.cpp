#include "UGameClockSubsystem.h"

void UGameClockSubsystem::Advance(float DeltaSeconds)
{
    if (bClockPaused) return;
    Accumulator += DeltaSeconds * (float)TicksPerSecond;
    int32 Ticks = (int32)Accumulator;
    if (Ticks == 0) return;
    Accumulator -= (float)Ticks;
    TotalTicks  += Ticks;
}

void UGameClockSubsystem::SetTicks(int64 Ticks)
{
    TotalTicks  = Ticks;
    Accumulator = 0.f;
}

void UGameClockSubsystem::PauseClock()  { bClockPaused = true;  }
void UGameClockSubsystem::ResumeClock() { bClockPaused = false; }

FGameDateTime UGameClockSubsystem::GetDateTime() const
{
    // 總遊戲分鐘數（0 = 5000年 1月1日 00:00 星期一）
    const int64 TotalMinutes = TotalTicks / TicksPerMinute;

    const int64 MinuteOfDay  = TotalMinutes % 1440;
    const int64 Hour         = MinuteOfDay  / 60;
    const int64 Minute       = MinuteOfDay  % 60;

    const int64 TotalDays    = TotalMinutes / 1440;
    const int64 DayOfWeek    = TotalDays % 7;            // 0=一,1=二…6=日

    const int64 YearIdx      = TotalDays / DaysPerYear;  // 從 StartYear 起算的年數
    const int64 DayOfYear    = TotalDays % DaysPerYear;  // 0-based
    const int64 MonthIdx     = DayOfYear / DaysPerMonth; // 0-based
    const int64 DayOfMonth   = DayOfYear % DaysPerMonth; // 0-based

    FGameDateTime DT;
    DT.Year      = (int32)(StartYear + YearIdx);
    DT.Month     = (int32)(MonthIdx + 1);
    DT.Day       = (int32)(DayOfMonth + 1);
    DT.Hour      = (int32)Hour;
    DT.Minute    = (int32)Minute;
    DT.DayOfWeek = (int32)DayOfWeek;
    return DT;
}

FString UGameClockSubsystem::GetFormattedTime() const
{
    const FGameDateTime DT = GetDateTime();

    static const TCHAR* DayNames[] =
    {
        TEXT("星期一"), TEXT("星期二"), TEXT("星期三"), TEXT("星期四"),
        TEXT("星期五"), TEXT("星期六"), TEXT("星期日")
    };
    const TCHAR* DayName = DayNames[FMath::Clamp(DT.DayOfWeek, 0, 6)];

    return FString::Printf(
        TEXT("蒼和%04d年 %02d月%02d日 %02d：%02d %s"),
        DT.Year, DT.Month, DT.Day, DT.Hour, DT.Minute, DayName);
}

bool UGameClockSubsystem::IsDay() const
{
    const int32 TickOfDay = (int32)(TotalTicks % TicksPerDay);
    return TickOfDay >= DawnTickOfDay && TickOfDay < DuskTickOfDay;
}
