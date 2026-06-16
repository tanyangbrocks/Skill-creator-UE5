#pragma once
#include "CoreMinimal.h"
#include "SpellArray.h"
#include "SpellLoadout.generated.h"

// 對應 Godot SpellLoadout.cs
// 一組配置：MaxSlots 個主動欄位 + 最多 MaxPassiveSlots 個被動技能
USTRUCT(BlueprintType)
struct FSpellLoadout
{
    GENERATED_BODY()

    static constexpr int32 MaxSlots        = 10;
    static constexpr int32 MaxPassiveSlots = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SpellLoadout")
    TArray<FSpellArray> Slots;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SpellLoadout")
    TArray<FSpellArray> PassiveSpells;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SpellLoadout")
    int32 ActiveIndex = 0;

    FSpellLoadout() { Slots.SetNum(MaxSlots); }

    bool IsSlotEmpty(int32 i) const
    {
        return !Slots.IsValidIndex(i) || !Slots[i].IsValid();
    }

    const FSpellArray& GetActiveSpell() const
    {
        return Slots.IsValidIndex(ActiveIndex) ? Slots[ActiveIndex] : Slots[0];
    }

    const FSpellArray& GetSlot(int32 i) const
    {
        return Slots.IsValidIndex(i) ? Slots[i] : Slots[0];
    }

    void SetSlot(int32 i, const FSpellArray& Spell)
    {
        if (Slots.IsValidIndex(i))
            Slots[i] = Spell;
    }

    FString SlotLabel(int32 i) const
    {
        if (!Slots.IsValidIndex(i)) return FString();
        return Slots[i].Name.IsEmpty()
            ? FString::Printf(TEXT("slot_%d"), i)
            : Slots[i].Name;
    }

    bool AddPassive(const FSpellArray& Spell)
    {
        if (PassiveSpells.Num() >= MaxPassiveSlots) return false;
        PassiveSpells.Add(Spell);
        return true;
    }

    bool RemovePassive(const FString& SpellName)
    {
        return PassiveSpells.RemoveAll(
            [&](const FSpellArray& S){ return S.Name == SpellName; }) > 0;
    }

    const FSpellArray* GetPassive(int32 i) const
    {
        return PassiveSpells.IsValidIndex(i) ? &PassiveSpells[i] : nullptr;
    }

    bool ReplacePassive(int32 i, const FSpellArray& Spell)
    {
        if (!PassiveSpells.IsValidIndex(i)) return false;
        PassiveSpells[i] = Spell;
        return true;
    }

    void ClearAll()
    {
        for (FSpellArray& S : Slots) S = FSpellArray();
        PassiveSpells.Empty();
        ActiveIndex = 0;
    }
};
