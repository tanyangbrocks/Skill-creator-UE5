#pragma once
#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

// UBorder::SetBrushColor() 依賴 T_DefaultDiffuse_D 這個引擎內建資源，UE5.7 此資源失效，
// 呼叫 SetBrushColor() 設定的顏色完全不會正確顯示（Bug H-4）。正確修法是改用
// SetBrush({DrawAs=RoundedBox, TintColor=color})——RoundedBox 是純程式繪製，不需要任何
// 貼圖資源就能正確渲染。原本每個檔案各自複製一份同名 static helper，Unity Build 把多個
// .cpp 併入同一個翻譯單元時會撞名（2026-06-23 實測撞過），改成這個共用 inline 版本。
// 2026-06-23 修復：FSlateBrushOutlineSettings 的預設建構子（SlateBrush.h:135-138）是
// RoundingType=HalfHeightRadius——這個模式下圓角半徑直接等於 brush 高度的一半，完全無視
// CornerRadii 設成 0。套在小元件（熱鍵欄格/血條）上不明顯，但套在覆蓋全螢幕的背景 panel
// 上，圓角半徑＝螢幕高度一半，整個矩形會被「吃成」橢圓形（使用者實機截圖證實：標題畫面
// 變形成橢圓，且因為背景蓋不住四個角落，底下角色/HUD/AVoxelWorldActor 已經在跑的世界從
// 縫隙露出來）。原本 SetBrushColor() 是純色矩形，沒有任何圓角，這裡明確設
// RoundingType=FixedRadius + CornerRadii=0，還原成同樣的零圓角矩形外觀，不要讓
// HalfHeightRadius 這個預設值偷偷生效。
inline FSlateBrush MakeSolidBrush(FLinearColor C)
{
    FSlateBrush B;
    B.DrawAs    = ESlateBrushDrawType::RoundedBox;
    B.TintColor = FSlateColor(C);
    B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    B.OutlineSettings.CornerRadii  = FVector4(0.0, 0.0, 0.0, 0.0);
    return B;
}
