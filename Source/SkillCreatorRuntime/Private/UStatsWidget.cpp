#include "UStatsWidget.h"
#include "UEquipmentComponent.h"
#include "ItemRegistry.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"

void UStatsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // 左上角面板（自動高度）
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
    Panel->SetBrushColor(FLinearColor(0.06f, 0.06f, 0.10f, 0.96f));
    Panel->SetPadding(FMargin(10.f, 8.f));
    Root->AddChild(Panel);
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Panel->Slot))
    {
        S->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
        S->SetPosition(FVector2D(8.f, 8.f));
        S->SetSize(FVector2D(290.f, 400.f));
        S->SetAutoSize(false);
    }

    ContentLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ContentLabel"));
    {
        FSlateFontInfo F = ContentLabel->GetFont();
        F.Size = 11;
        ContentLabel->SetFont(F);
    }
    ContentLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.90f)));
    Panel->AddChild(ContentLabel);
}

void UStatsWidget::Refresh(const FCharacterStats& Stats, float Hp, float MaxHp,
                            float Mp, float MaxMp, int32 Level, FString TierName,
                            const UEquipmentComponent* Equip)
{
    if (!ContentLabel) return;

    FString Wn  = TEXT("─");
    FString An  = TEXT("─");
    FString Acn = TEXT("─");

    if (Equip)
    {
        if (Equip->WeaponId    != EItemId::None) Wn  = FItemRegistry::Get(Equip->WeaponId).DisplayName.ToString();
        if (Equip->ArmorId     != EItemId::None) An  = FItemRegistry::Get(Equip->ArmorId).DisplayName.ToString();
        if (Equip->AccessoryId != EItemId::None) Acn = FItemRegistry::Get(Equip->AccessoryId).DisplayName.ToString();
    }

    float TotalMp = MaxMp + (Equip ? Equip->GetTotalMpBonus() : 0.f);

    FString Text =
        FString::Printf(TEXT("═══ 角色數值 [C 關閉] ═══\n")) +
        FString::Printf(TEXT("【LV %d  %s】\n"), Level, *TierName) +
        FString::Printf(TEXT("[生命/法力]\n")) +
        FString::Printf(TEXT("  HP %.0f  MP %.0f（裝備+%.0f）\n"), MaxHp, TotalMp, Equip ? Equip->GetTotalMpBonus() : 0.f) +
        FString::Printf(TEXT("  HP回復 %.1f/s  MP回復 %.1f/s\n"), Stats.HpRegenRate, Stats.MpRegenRate) +
        FString::Printf(TEXT("[戰鬥]\n")) +
        FString::Printf(TEXT("  力量 %.0f  物傷加成 %.0f%%\n"), Stats.Power, Stats.PhysicalDmgPct * 100.f) +
        FString::Printf(TEXT("  爆率 %.0f%%  爆傷倍率 ×%.1f\n"), Stats.CritRate * 100.f, Stats.CritDmgMult) +
        FString::Printf(TEXT("  吸血 %.0f%%  反傷 %.0f\n"), Stats.Lifesteal * 100.f, Stats.Thorns) +
        FString::Printf(TEXT("[防禦]\n")) +
        FString::Printf(TEXT("  基礎防禦 %.0f  裝備防禦 +%.0f\n"), Stats.BaseDefense, Equip ? Equip->GetTotalDefFlat() : 0.f) +
        FString::Printf(TEXT("  減傷 %.0f%%  抗暴 %.0f%%\n"), Stats.DamageReduction * 100.f, Stats.AntiCrit * 100.f) +
        FString::Printf(TEXT("[機動]\n")) +
        FString::Printf(TEXT("  移速 ×%.2f  攻速 ×%.2f\n"), Stats.MoveSpeedMult, Stats.AtkSpeedMult) +
        FString::Printf(TEXT("  命中 %.0f%%  閃避 %.0f%%\n"), Stats.HitRate * 100.f, Stats.DodgeRate * 100.f) +
        FString::Printf(TEXT("[裝備]\n")) +
        FString::Printf(TEXT("  武器：%s  防具：%s  飾品：%s\n"), *Wn, *An, *Acn) +
        FString::Printf(TEXT("[天賦]\n")) +
        FString::Printf(TEXT("  體魄 %d  肌力 %d  耐力 %d\n"), Stats.TalentConstitution, Stats.TalentStrength, Stats.TalentEndurance) +
        FString::Printf(TEXT("  敏捷 %d  智慧 %d  魅力 %d  幸運 %d"), Stats.TalentAgility, Stats.TalentWisdom, Stats.TalentCharisma, Stats.TalentLuck);

    ContentLabel->SetText(FText::FromString(Text));
}
