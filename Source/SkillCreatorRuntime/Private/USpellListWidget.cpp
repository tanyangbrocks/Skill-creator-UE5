#include "USpellListWidget.h"
#include "ASkillCreatorCharacter.h"
#include "USpellCaster.h"
#include "SpellArray.h"
#include "SpellGroup.h"
#include "SpellDescriptionGenerator.h"
#include "ElementType.h"
#include "GameFramework/PlayerController.h"

// ─── 私有工具 ────────────────────────────────────────────────────────────────

ASkillCreatorCharacter* USpellListWidget::GetOwnerCharacter() const
{
    APlayerController* PC = GetOwningPlayer();
    return PC ? Cast<ASkillCreatorCharacter>(PC->GetPawn()) : nullptr;
}

// ─── 初始化 ──────────────────────────────────────────────────────────────────

void USpellListWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshSpellList();
}

// ─── Public API ──────────────────────────────────────────────────────────────

void USpellListWidget::RefreshSpellList()
{
    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    if (!Char || !Char->SpellCasterComp) return;

    const FSpellGroup&   Group   = Char->SpellCasterComp->SpellGroups;
    const FSpellLoadout& Loadout = Group.GetActiveLoadout();
    const int32 ActiveIdx        = Loadout.ActiveIndex;

    CachedSlots.Reset(FSpellLoadout::MaxSlots);
    for (int32 i = 0; i < FSpellLoadout::MaxSlots; ++i)
    {
        FSpellSlotDisplayInfo Info;
        Info.SlotIndex = i;

        if (Loadout.Slots.IsValidIndex(i))
        {
            const FSpellArray& Spell = Loadout.Slots[i];

            Info.bIsEmpty      = !Spell.IsValid();
            Info.bIsActiveSlot = (i == ActiveIdx);

            if (!Info.bIsEmpty)
            {
                Info.SpellName      = Spell.Name;
                Info.BaseMpCost     = Spell.BaseMpCost;
                Info.bIsPassive     = Spell.IsPassive();
                Info.StructuredDesc = FSpellDescriptionGenerator::GenerateStructured(Spell);

                // 主要元素
                switch (Spell.SpellElement)
                {
                case ESkillElementType::None:    Info.ElementName = TEXT("");    break;
                case ESkillElementType::Metal:   Info.ElementName = TEXT("金");  break;
                case ESkillElementType::Wood:    Info.ElementName = TEXT("木");  break;
                case ESkillElementType::Water:   Info.ElementName = TEXT("水");  break;
                case ESkillElementType::Fire:    Info.ElementName = TEXT("火");  break;
                case ESkillElementType::Earth:   Info.ElementName = TEXT("土");  break;
                case ESkillElementType::Ice:     Info.ElementName = TEXT("冰");  break;
                case ESkillElementType::Wind:    Info.ElementName = TEXT("風");  break;
                case ESkillElementType::Light:   Info.ElementName = TEXT("光");  break;
                case ESkillElementType::Dark:    Info.ElementName = TEXT("暗");  break;
                case ESkillElementType::Thunder: Info.ElementName = TEXT("雷");  break;
                case ESkillElementType::Poison:  Info.ElementName = TEXT("毒");  break;
                default: break;
                }
            }
        }
        else
        {
            Info.bIsEmpty = true;
        }

        CachedSlots.Add(MoveTemp(Info));
    }

    OnSpellListRefreshed(CachedSlots, Group.ActiveGroupIndex);
}

int32 USpellListWidget::GetActiveGroupIndex() const
{
    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    if (!Char || !Char->SpellCasterComp) return 0;
    return Char->SpellCasterComp->SpellGroups.ActiveGroupIndex;
}

void USpellListWidget::SetActiveGroup(int32 GroupIndex)
{
    ASkillCreatorCharacter* Char = GetOwnerCharacter();
    if (!Char || !Char->SpellCasterComp) return;

    Char->SpellCasterComp->SpellGroups.SetActiveGroup(GroupIndex);
    RefreshSpellList();
}
