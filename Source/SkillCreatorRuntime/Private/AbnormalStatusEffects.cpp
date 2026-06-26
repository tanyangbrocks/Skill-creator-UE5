#include "AbnormalStatusEffects.h"

void FWetStatus::OnApply(IElementalTarget* Target)
{
    if (Target)
        Target->ApplyElementalAuraImmediate(ESkillElementType::Water, RemainingDuration);
}
