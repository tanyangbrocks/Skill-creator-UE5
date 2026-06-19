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

        // 2026-06-19 稽核：Mode 欄位先前儲存了但從未被讀取，"Halve" 模式形同無作用。
        // 目前沒有任何積木/Totem UI 會真的產生 "Halve" 值（Mode 預設空字串），所以這不是
        // 「目前遊玩中算錯傷害」的可見 bug，是「欄位存在但邏輯沒接上」的死碼問題，現在補上。
        // Mode 未設定（空字串）時維持原本行為：CapValue<=0 全擋，否則限制單次傷害上限。
        if (E.Mode == TEXT("Halve"))
            Damage *= 0.5f;
        else if (E.Mode == TEXT("Cancel"))
            Damage = 0.f;
        else
            Damage = (E.CapValue <= 0.f) ? 0.f : FMath::Min(Damage, E.CapValue);

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
