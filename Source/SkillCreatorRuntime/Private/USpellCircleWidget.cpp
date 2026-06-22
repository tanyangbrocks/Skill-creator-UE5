#include "USpellCircleWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Styling/SlateTypes.h"

static constexpr float GCircleSizePx = 260.f;

// ─── 靜態工具 ──────────────────────────────────────────────────────────────────

FSlateBrush USpellCircleWidget::MakeRoundedBrush(const FLinearColor& BgColor,
                                                  const FLinearColor& BorderColor,
                                                  float               BorderWidth,
                                                  float               CornerRadius)
{
    FSlateBrush Brush;
    Brush.DrawAs   = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = FSlateColor(BgColor);
    Brush.OutlineSettings.Color        = FSlateColor(BorderColor);
    Brush.OutlineSettings.Width        = BorderWidth;
    Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    Brush.OutlineSettings.CornerRadii  = FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius);
    return Brush;
}

// ─── SetupSpell ───────────────────────────────────────────────────────────────
// 對應 Godot SpellListUI.cs:378-413 MakeSpellCircle

void USpellCircleWidget::SetupSpell(const FString& SpellName,
                                     const FString& TooltipText,
                                     bool           bIsPassive,
                                     bool           bIsOverLimit,
                                     TFunction<void()>               InOnClicked,
                                     TFunction<void(const FString&)> InOnHover,
                                     TFunction<void()>               InOnUnhover)
{
    CachedTooltipText  = TooltipText;
    OnClickCallback    = MoveTemp(InOnClicked);
    OnHoverCallback    = MoveTemp(InOnHover);
    OnUnhoverCallback  = MoveTemp(InOnUnhover);

    // Godot SpellListUI.cs:385-392 顏色定義
    FLinearColor BgNorm   = bIsOverLimit ? FLinearColor(0.05f,0.05f,0.05f,1.f) : FLinearColor(0.05f,0.08f,0.18f,1.f);
    FLinearColor BgHover  = bIsOverLimit ? FLinearColor(0.10f,0.10f,0.10f,1.f) : FLinearColor(0.10f,0.16f,0.34f,1.f);
    FLinearColor BrdNorm  = bIsOverLimit ? FLinearColor(0.25f,0.25f,0.25f,1.f)
                          : bIsPassive   ? FLinearColor(0.15f,0.88f,0.75f,1.f)
                                         : FLinearColor(0.30f,0.58f,1.00f,1.f);
    FLinearColor BrdHover = bIsOverLimit ? FLinearColor(0.35f,0.35f,0.35f,1.f)
                          : bIsPassive   ? FLinearColor(0.28f,1.00f,0.88f,1.f)
                                         : FLinearColor(0.55f,0.78f,1.00f,1.f);

    FButtonStyle Style;
    Style.Normal  = MakeRoundedBrush(BgNorm,  BrdNorm,  5.f);
    Style.Hovered = MakeRoundedBrush(BgHover, BrdHover, 7.f);
    Style.Pressed = MakeRoundedBrush(BgHover, BrdHover, 7.f);

    CircleBtn = WidgetTree->ConstructWidget<UButton>();
    CircleBtn->SetStyle(Style);
    CircleBtn->OnClicked.AddDynamic  (this, &USpellCircleWidget::HandleClicked);
    CircleBtn->OnHovered.AddDynamic  (this, &USpellCircleWidget::HandleHovered);
    CircleBtn->OnUnhovered.AddDynamic(this, &USpellCircleWidget::HandleUnhovered);

    // Godot SpellListUI.cs:399-410 圓球文字
    FString Display = SpellName.IsEmpty() ? TEXT("（未命名）") : SpellName;
    if (bIsOverLimit) Display += TEXT("\n超過上限");

    UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>();
    Lbl->SetText(FText::FromString(Display));
    Lbl->SetJustification(ETextJustify::Center);
    FSlateFontInfo F = Lbl->GetFont(); F.Size = 16; Lbl->SetFont(F);
    Lbl->SetColorAndOpacity(FSlateColor(bIsOverLimit ? FLinearColor(0.42f,0.42f,0.42f) : FLinearColor::White));
    CircleBtn->AddChild(Lbl);

    // SizeBox 強制 260×260（對應 Godot btn.CustomMinimumSize = new Vector2(CircleSize, CircleSize)）
    USizeBox* SBox = WidgetTree->ConstructWidget<USizeBox>();
    SBox->SetWidthOverride (GCircleSizePx);
    SBox->SetHeightOverride(GCircleSizePx);
    SBox->SetContent(CircleBtn);
    WidgetTree->RootWidget = SBox;
}

// ─── SetupAdd ─────────────────────────────────────────────────────────────────
// 對應 Godot SpellListUI.cs:415-436 MakeAddCircle（尺寸 = CircleSize = 260）

void USpellCircleWidget::SetupAdd(TFunction<void()> InOnClicked)
{
    OnClickCallback = MoveTemp(InOnClicked);

    FButtonStyle Style;
    Style.Normal  = MakeRoundedBrush(FLinearColor(0.10f,0.18f,0.10f,1.f), FLinearColor(0.30f,0.70f,0.30f,1.f), 2.f);
    Style.Hovered = MakeRoundedBrush(FLinearColor(0.16f,0.26f,0.16f,1.f), FLinearColor(0.45f,0.90f,0.45f,1.f), 3.f);
    Style.Pressed = MakeRoundedBrush(FLinearColor(0.08f,0.14f,0.08f,1.f), FLinearColor(0.28f,0.65f,0.28f,1.f), 2.f);

    CircleBtn = WidgetTree->ConstructWidget<UButton>();
    CircleBtn->SetStyle(Style);
    CircleBtn->OnClicked.AddDynamic(this, &USpellCircleWidget::HandleClicked);

    UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>();
    Lbl->SetText(FText::FromString(TEXT("+")));
    Lbl->SetJustification(ETextJustify::Center);
    FSlateFontInfo F = Lbl->GetFont(); F.Size = 28; Lbl->SetFont(F);
    Lbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 1.0f, 0.5f)));
    CircleBtn->AddChild(Lbl);

    USizeBox* SBox = WidgetTree->ConstructWidget<USizeBox>();
    SBox->SetWidthOverride (GCircleSizePx);
    SBox->SetHeightOverride(GCircleSizePx);
    SBox->SetContent(CircleBtn);
    WidgetTree->RootWidget = SBox;
}

// ─── Event handlers ───────────────────────────────────────────────────────────

void USpellCircleWidget::HandleClicked()  { if (OnClickCallback)   OnClickCallback(); }
void USpellCircleWidget::HandleHovered()  { if (OnHoverCallback)   OnHoverCallback(CachedTooltipText); }
void USpellCircleWidget::HandleUnhovered(){ if (OnUnhoverCallback) OnUnhoverCallback(); }
