#include "ActionBus.h"

void FActionBus::RegisterFilter(FName FilterType, FName Mode, bool bOneShot,
                                float Threshold, float CapValue)
{
    FActionFilterEntry E;
    E.FilterType = FilterType;
    E.Mode       = Mode;
    E.bOneShot   = bOneShot;
    E.Threshold  = Threshold;
    E.CapValue   = CapValue;
    // one-shot 項目不計時（Remaining<0）；其餘用 Threshold 作為持續秒數
    E.Remaining  = bOneShot ? -1.f : FMath::Max(0.001f, Threshold);
    Entries.Add(E);
}

void FActionBus::Update(float DeltaTime)
{
    for (int32 i = Entries.Num() - 1; i >= 0; --i)
    {
        FActionFilterEntry& E = Entries[i];
        if (E.Remaining < 0.f) continue; // one-shot 不受時間影響
        E.Remaining -= DeltaTime;
        if (E.Remaining <= 0.f)
            Entries.RemoveAt(i);
    }
}

float FActionBus::DispatchPlayerDamage(float InDamage)
{
    float Damage = InDamage;
    for (int32 i = Entries.Num() - 1; i >= 0; --i)
    {
        FActionFilterEntry& E = Entries[i];
        if (E.FilterType != TEXT("DamageShield")) continue;
        if (E.CapValue <= 0.f)
            Damage = 0.f;                          // 全擋
        else
            Damage = FMath::Min(Damage, E.CapValue); // 限制單次傷害上限
        if (E.bOneShot)
            Entries.RemoveAt(i);
    }
    return Damage;
}

bool FActionBus::DispatchPlayerDeath()
{
    for (int32 i = Entries.Num() - 1; i >= 0; --i)
    {
        FActionFilterEntry& E = Entries[i];
        if (E.FilterType != TEXT("DeathGuard")) continue;
        Entries.RemoveAt(i); // DeathGuard 永遠是 one-shot
        return true;         // 死亡取消
    }
    return false;
}
