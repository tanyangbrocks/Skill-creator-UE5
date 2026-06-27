#include "UAbnormalStatusBarWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "SlateBrushHelpers.h"

static UTexture2D* LoadStatusIcon(const FName& StatusId)
{
    const FString S = StatusId.ToString();
    const FString Path = FString::Printf(TEXT("/Game/Icons/STA_%s.STA_%s"), *S, *S);
    return LoadObject<UTexture2D>(nullptr, *Path);
}

static void PinIcon(UWidget* W, float X, float Y, float W2, float H)
{
    if (!W) return;
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(W->Slot))
    {
        S->SetAnchors(FAnchors(0.f, 0.f));
        S->SetPosition(FVector2D(X, Y));
        S->SetSize(FVector2D(W2, H));
        S->SetAlignment(FVector2D::ZeroVector);
    }
}

void UAbnormalStatusBarWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    IconBorders.Reserve(MaxIcons);
    IconImages.Reserve(MaxIcons);
    IconLabels.Reserve(MaxIcons);
    StackLabels.Reserve(MaxIcons);
    TimerLabels.Reserve(MaxIcons);

    for (int32 i = 0; i < MaxIcons; i++)
    {
        // 外框 Border（固定 IconSize × IconSize）
        UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
        Border->SetPadding(FMargin(0.f));
        Border->SetBrush(MakeSolidBrush(FillColor(EAbnormalPolarity::Negative)));
        Root->AddChild(Border);
        PinIcon(Border, i * (IconSize + 2.f), 0.f, IconSize, IconSize);
        IconBorders.Add(Border);

        // 內層 Canvas 承載圖示 + 3 個文字
        UCanvasPanel* Inner = WidgetTree->ConstructWidget<UCanvasPanel>();
        Border->AddChild(Inner);
        if (UBorderSlot* BS = Cast<UBorderSlot>(Inner->Slot))
        {
            BS->SetHorizontalAlignment(HAlign_Fill);
            BS->SetVerticalAlignment(VAlign_Fill);
        }

        // 狀態圖示（最底層，有圖示時填滿；無圖示時 Collapsed，改顯示縮寫文字）
        UImage* StatusImg = WidgetTree->ConstructWidget<UImage>();
        Inner->AddChild(StatusImg);
        if (UCanvasPanelSlot* IS = Cast<UCanvasPanelSlot>(StatusImg->Slot))
        {
            IS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
            IS->SetOffsets(FMargin(2.f, 2.f, 2.f, 2.f));
        }
        StatusImg->SetVisibility(ESlateVisibility::Collapsed);
        IconImages.Add(StatusImg);

        auto MkTxt = [&](FVector2D Pos, int32 Sz) -> UTextBlock*
        {
            UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
            FSlateFontInfo F = T->GetFont(); F.Size = Sz; T->SetFont(F);
            T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
            Inner->AddChild(T);
            if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(T->Slot))
            {
                CS->SetAnchors(FAnchors(0.f, 0.f));
                CS->SetPosition(Pos);
                CS->SetAutoSize(true);
            }
            return T;
        };

        IconLabels.Add(MkTxt(FVector2D(1.f, 5.f),  8));   // 縮寫（居中）
        StackLabels.Add(MkTxt(FVector2D(12.f, 0.f), 7));  // 右上角疊加數
        TimerLabels.Add(MkTxt(FVector2D(1.f, 12.f), 7)); // 左下角計時

        // 初始隱藏
        Border->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UAbnormalStatusBarWidget::UpdateStatuses(const TArray<FStatusDisplaySnapshot>& Snaps)
{
    const int32 N = FMath::Min(Snaps.Num(), MaxIcons);

    for (int32 i = 0; i < MaxIcons; i++)
    {
        if (!IconBorders.IsValidIndex(i) || !IconBorders[i]) continue;
        UBorder* B = IconBorders[i];

        if (i >= N)
        {
            B->SetVisibility(ESlateVisibility::Collapsed);
            continue;
        }

        const FStatusDisplaySnapshot& S = Snaps[i];
        B->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        B->SetBrush(MakeSolidBrush(FillColor(S.Polarity)));

        // 位置：visible icon 位置可能因 Collapsed 前面項變化，需重新設
        PinIcon(B, i * (IconSize + 2.f), 0.f, IconSize, IconSize);

        // 圖示（有 /Game/Icons/STA_{StatusId} 時顯示圖示，否則 fallback 到文字縮寫）
        UTexture2D* Tex = LoadStatusIcon(S.StatusId);
        if (IconImages.IsValidIndex(i) && IconImages[i])
        {
            if (Tex)
            {
                IconImages[i]->SetBrushFromTexture(Tex, false);
                IconImages[i]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
                if (IconLabels.IsValidIndex(i) && IconLabels[i])
                    IconLabels[i]->SetVisibility(ESlateVisibility::Collapsed);
            }
            else
            {
                IconImages[i]->SetVisibility(ESlateVisibility::Collapsed);
                if (IconLabels.IsValidIndex(i) && IconLabels[i])
                {
                    IconLabels[i]->SetText(FText::FromString(S.DisplayName.ToString().Left(2)));
                    IconLabels[i]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
                }
            }
        }

        // 疊加數
        if (StackLabels.IsValidIndex(i) && StackLabels[i])
        {
            if (S.StackCount > 1)
            {
                StackLabels[i]->SetText(FText::AsNumber(S.StackCount));
                StackLabels[i]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            }
            else
                StackLabels[i]->SetVisibility(ESlateVisibility::Collapsed);
        }

        // 倒數計時（< 10s 顯示）
        if (TimerLabels.IsValidIndex(i) && TimerLabels[i])
        {
            if (S.RemainingDuration < 10.f)
            {
                TimerLabels[i]->SetText(FText::FromString(
                    FString::Printf(TEXT("%.0f"), S.RemainingDuration)));
                TimerLabels[i]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            }
            else
                TimerLabels[i]->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

FLinearColor UAbnormalStatusBarWidget::BorderColor(EAbnormalPolarity Polarity)
{
    return Polarity == EAbnormalPolarity::Positive
        ? FLinearColor(0.20f, 0.40f, 0.90f, 1.f)
        : FLinearColor(0.85f, 0.35f, 0.05f, 1.f);
}

FLinearColor UAbnormalStatusBarWidget::FillColor(EAbnormalPolarity Polarity)
{
    return Polarity == EAbnormalPolarity::Positive
        ? FLinearColor(0.06f, 0.08f, 0.22f, 0.88f)
        : FLinearColor(0.18f, 0.06f, 0.01f, 0.88f);
}
