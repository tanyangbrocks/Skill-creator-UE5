#include "USpellListWidget.h"
#include "ASkillCreatorCharacter.h"
#include "USpellCaster.h"
#include "SpellArray.h"
#include "SpellGroup.h"
#include "SpellDescriptionGenerator.h"
#include "ElementType.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"

static constexpr int32 MaxSlots  = 10;
static constexpr int32 MaxGroups =  5;

// ─── 私有工具 ────────────────────────────────────────────────────────────────

ASkillCreatorCharacter* USpellListWidget::GetOwnerCharacter() const
{
    APlayerController* PC = GetOwningPlayer();
    return PC ? Cast<ASkillCreatorCharacter>(PC->GetPawn()) : nullptr;
}

// ─── Widget 樹建構 ────────────────────────────────────────────────────────────

void USpellListWidget::BuildLayout()
{
    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
    WidgetTree->RootWidget = Root;

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
    Title->SetText(FText::FromString(TEXT("─── 技能欄 ───")));
    Root->AddChildToVerticalBox(Title);

    // 10 格技能插槽（橫排）
    SlotContainer = WidgetTree->ConstructWidget<UHorizontalBox>();
    Root->AddChildToVerticalBox(SlotContainer);
    SlotTexts.SetNum(MaxSlots);
    for (int32 i = 0; i < MaxSlots; ++i)
    {
        UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
        T->SetText(FText::FromString(TEXT("[空]")));
        if (UHorizontalBoxSlot* S = SlotContainer->AddChildToHorizontalBox(T))
            S->SetPadding(FMargin(4.f, 0.f));
        SlotTexts[i] = T;
    }

    // 5 個組別圓點（橫排）
    UTextBlock* GroupLabel = WidgetTree->ConstructWidget<UTextBlock>();
    GroupLabel->SetText(FText::FromString(TEXT("組別：")));
    Root->AddChildToVerticalBox(GroupLabel);

    GroupDotContainer = WidgetTree->ConstructWidget<UHorizontalBox>();
    Root->AddChildToVerticalBox(GroupDotContainer);
    GroupDotTexts.SetNum(MaxGroups);
    for (int32 g = 0; g < MaxGroups; ++g)
    {
        UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
        T->SetText(FText::FromString(TEXT("○")));
        if (UHorizontalBoxSlot* S = GroupDotContainer->AddChildToHorizontalBox(T))
            S->SetPadding(FMargin(4.f, 0.f));
        GroupDotTexts[g] = T;
    }

    // Tooltip 文字
    SlotTooltipText = WidgetTree->ConstructWidget<UTextBlock>();
    SlotTooltipText->SetText(FText::GetEmpty());
    SlotTooltipText->SetAutoWrapText(true);
    Root->AddChildToVerticalBox(SlotTooltipText);
}

// ─── 初始化 ──────────────────────────────────────────────────────────────────

void USpellListWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BuildLayout();
    RefreshSpellList();
}

// ─── BlueprintNativeEvent 預設實作 ───────────────────────────────────────────

void USpellListWidget::OnSpellListRefreshed_Implementation(
    const TArray<FSpellSlotDisplayInfo>& Slots, int32 ActiveGroup)
{
    // 更新插槽文字
    for (int32 i = 0; i < MaxSlots; ++i)
    {
        if (!SlotTexts.IsValidIndex(i) || !SlotTexts[i]) continue;

        if (Slots.IsValidIndex(i) && !Slots[i].bIsEmpty)
        {
            const FSpellSlotDisplayInfo& S = Slots[i];
            const FString Marker = S.bIsActiveSlot ? TEXT("▶") : TEXT("·");
            SlotTexts[i]->SetText(FText::FromString(
                FString::Printf(TEXT("%s%s"), *Marker, *S.SpellName)));
        }
        else
        {
            SlotTexts[i]->SetText(FText::FromString(TEXT("[空]")));
        }
    }

    // 更新組別圓點（作用中 = ●，其餘 = ○）
    for (int32 g = 0; g < MaxGroups; ++g)
    {
        if (!GroupDotTexts.IsValidIndex(g) || !GroupDotTexts[g]) continue;
        GroupDotTexts[g]->SetText(FText::FromString(g == ActiveGroup ? TEXT("●") : TEXT("○")));
    }

    // 清空 Tooltip
    if (SlotTooltipText) SlotTooltipText->SetText(FText::GetEmpty());
}

void USpellListWidget::OnSlotHovered_Implementation(const FSpellSlotDisplayInfo& HoveredSlot)
{
    if (!SlotTooltipText) return;
    if (HoveredSlot.bIsEmpty)
    {
        SlotTooltipText->SetText(FText::FromString(TEXT("（空格）")));
        return;
    }

    const FString Desc = FString::Printf(
        TEXT("%s  [%s]  MP: %.0f%s\n%s"),
        *HoveredSlot.SpellName,
        *HoveredSlot.ElementName,
        HoveredSlot.BaseMpCost,
        HoveredSlot.bIsPassive ? TEXT(" 被動") : TEXT(""),
        *HoveredSlot.StructuredDesc);
    SlotTooltipText->SetText(FText::FromString(Desc));
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
