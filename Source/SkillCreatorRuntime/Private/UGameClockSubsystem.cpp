#include "UGameClockSubsystem.h"

void UGameClockSubsystem::Advance(float DeltaSeconds)
{
    Accumulator += DeltaSeconds * (float)TicksPerSecond;
    int32 Ticks = (int32)Accumulator;
    if (Ticks == 0) return;
    Accumulator -= (float)Ticks;
    TotalTicks  += Ticks;
    DayCount     = (int32)(TotalTicks / TicksPerDay);
}

void UGameClockSubsystem::Reset()
{
    TotalTicks  = 0;
    DayCount    = 0;
    Accumulator = 0.f;
}
