# 投擲瞄準弧線實作計畫

> 撰寫日期：2026-06-28  
> 依據：`docs/plugin-assessment.html` §「投擲瞄準弧線」  
> 關鍵決策：**不使用 Niagara**（需 Editor 建立 .uasset），改用 HUD `DrawLine()` + `DrawRect()`  
>           在螢幕空間繪製弧線預覽 — 純 C++，零 Editor 操作。

---

## 一、現有架構評估

### 投擲流程（目前）

```
玩家按 F →
ASkillCreatorPlayerController → HUD->StartThrowCharge()
UPhysicalThrowWidget::StartCharging() [bCharging=true, CurrentT=0]
     ↓ NativeTick 每幀：CurrentT += DeltaTime → 計算 Pct(0→1→0) → 更新滑桿位置
玩家放開 F →
HUD->FinishThrowCharge(Char)
ThrowWidget->StopCharging() → LockedPct = 當前 Pct
Speed = 200 * TileSizeCm * PowerPct * Strength / Mass
Char->EndCarry(AimDir * Speed)  // AimDir = ControlRotation.Vector()
```

### 問題
- `ThrowWidget::GetPowerPct()` 在 `bCharging = true` 時回傳 `0.f`（只有 StopCharging 後才有值）
- HUD 的 `DrawHUD()` 沒有在充電期間繪製任何弧線

### 關鍵觀察

1. **瞄準方向 = ControlRotation.Vector()**：這是相機朝向，即玩家實際想丟的方向
2. **速度公式已知**：`BaseSpeed * Pct * Strength / Mass`，可在充電期間實時預覽
3. **Niagara Ribbon 替代方案**：`DrawHUD()` 中 `DrawLine()` 折線 → 無需任何 Editor 資產
4. **`SuggestProjectileVelocity()`** 是「給定目標反求速度」的函數，本系統是「已知速度求落點」，不適合使用；保留為可選的「點擊目標投擲」模式（ARC-opt）

---

## 二、修改清單

| 檔案 | 修改 |
|------|------|
| `Source/SkillCreatorRuntime/Public/UPhysicalThrowWidget.h` | 新增 `GetCurrentChargePct()` |
| `Source/SkillCreatorRuntime/Private/UPhysicalThrowWidget.cpp` | 實作 `GetCurrentChargePct()` |
| `Source/SkillCreatorRuntime/Public/ASkillCreatorHUD.h` | 新增 `bIsChargingThrow`、`DrawThrowArc()` |
| `Source/SkillCreatorRuntime/Private/ASkillCreatorHUD.cpp` | `StartThrowCharge`/`FinishThrowCharge` 同步狀態 + `DrawHUD()` 呼叫 `DrawThrowArc()` |

---

## 三、實作步驟

### ARC-0：UPhysicalThrowWidget 新增 GetCurrentChargePct()

**`UPhysicalThrowWidget.h`** 新增 public：
```cpp
// ARC-0：充電期間回傳當前指針位置（0..1），供 HUD 弧線預覽
// 充電中：從 CurrentT 計算；已鎖定：回傳 LockedPct；未啟動：回傳 0
float GetCurrentChargePct() const;
```

**`UPhysicalThrowWidget.cpp`** 新增：
```cpp
float UPhysicalThrowWidget::GetCurrentChargePct() const
{
    if (bCharging)
    {
        const float NormT = CurrentT / (CycleTime * 0.5f);
        return FMath::Clamp(NormT <= 1.f ? NormT : 2.f - NormT, 0.f, 1.f);
    }
    return LockedPct;
}
```

> **為什麼需要這個**：`NativeTick` 雖然每幀更新 UI，但 `CurrentT` 是 `private`；  
> 現有的 `GetPowerPct()` 充電期間故意回傳 0，避免在 `FinishThrowCharge` 前誤拿值。  
> 弧線預覽需要實時讀取充電進度，所以另加 `GetCurrentChargePct()`，語意明確分離。

### ARC-1：ASkillCreatorHUD — 充電狀態追蹤

**`ASkillCreatorHUD.h`** 新增 private：
```cpp
// ARC-1：投擲充電中旗標（StartThrowCharge 設 true，FinishThrowCharge 清 false）
bool bIsChargingThrow = false;
void DrawThrowArc();  // DrawHUD() 中呼叫
```

**`ASkillCreatorHUD.cpp`**：

```cpp
void ASkillCreatorHUD::StartThrowCharge()
{
    if (ThrowWidget)
        ThrowWidget->StartCharging();
    bIsChargingThrow = true;    // ARC-1
}

void ASkillCreatorHUD::FinishThrowCharge(ASkillCreatorCharacter* Char)
{
    bIsChargingThrow = false;   // ARC-1
    // ... 原有邏輯不變 ...
}
```

### ARC-2：DrawThrowArc() — 軌跡計算 + 螢幕投影

**`ASkillCreatorHUD.cpp`** 新增：

```cpp
void ASkillCreatorHUD::DrawThrowArc()
{
    if (!bIsChargingThrow || !ThrowWidget) return;

    APlayerController* PC = GetOwningPlayerController();
    ASkillCreatorCharacter* Char = PC ? PC->GetPawn<ASkillCreatorCharacter>() : nullptr;
    if (!Char) return;

    // ── 讀取投擲參數 ──
    const float ChargePct = ThrowWidget->GetCurrentChargePct();  // ARC-0
    if (ChargePct < 0.01f) return;  // 還沒開始充電，不畫

    IPhysicalPickable* P = Cast<APhysicalItemActor>(Char->CarriedActor.Get());
    if (!P) P = Cast<ADebrisActor>(Char->CarriedActor.Get());
    const float Mass     = P ? P->GetMass() : 1.f;
    const float Strength = Char->Stats.Strength;

    constexpr float BaseSpeed = 200.f * WorldScale::TileSizeCm;
    const float Speed = FMath::Clamp(
        BaseSpeed * ChargePct * Strength / FMath::Max(0.1f, Mass),
        0.f, 5000.f);

    // ── 初始速度向量（同 FinishThrowCharge 的 AimDir）──
    FVector AimDir = PC->GetControlRotation().Vector();
    FVector LaunchPos = Char->GetActorLocation()
        + AimDir * WorldScale::TileSizeCm * 2.f
        + FVector(0.f, 0.f, WorldScale::TileSizeCm);  // 略高於角色中心
    FVector Velocity = AimDir * Speed;

    // ── 拋體模擬 ──
    // UE5：Z 向上為正，GetGravityZ() 約 = -980.f（cm/s²）
    const FVector Gravity(0.f, 0.f, GetWorld()->GetGravityZ());
    constexpr float Dt      = 0.06f;  // 每步 60 ms
    constexpr float MaxTime = 5.f;    // 最多模擬 5 秒
    constexpr int32 MaxPts  = 84;     // 5s / 0.06s

    TArray<FVector2D> ScreenPts;
    ScreenPts.Reserve(MaxPts);

    AVoxelWorldActor* VWA = AVoxelWorldActor::FindInWorld(GetWorld());
    FVector LastPos = LaunchPos;

    for (float T = Dt; T <= MaxTime; T += Dt)
    {
        FVector Pos = LaunchPos + Velocity * T + 0.5f * Gravity * T * T;

        // 著地判斷：查 tile
        if (VWA)
        {
            using WS = WorldScale;
            FGridPos GP = WS::WorldToTile(Pos);
            EMaterialType TileMat = VWA->GetTileWorld()->GetTile(GP.X, GP.Y, GP.Z);
            if (TileMat != EMaterialType::Air && TileMat != EMaterialType::Water)
            {
                // 落點在 LastPos（前一步仍是空氣）
                FVector2D FinalScreen;
                if (PC->ProjectWorldLocationToScreen(LastPos, FinalScreen, true))
                    ScreenPts.Add(FinalScreen);
                break;
            }
        }

        FVector2D SP;
        if (PC->ProjectWorldLocationToScreen(Pos, SP, true))
            ScreenPts.Add(SP);

        LastPos = Pos;
    }

    if (ScreenPts.Num() < 2) return;

    // ── 繪製弧線折線 ──
    const FLinearColor ArcColor(1.f, 0.85f, 0.1f, 0.85f);  // 黃色半透明
    for (int32 i = 0; i + 1 < ScreenPts.Num(); ++i)
    {
        DrawLine(
            ScreenPts[i  ].X, ScreenPts[i  ].Y,
            ScreenPts[i+1].X, ScreenPts[i+1].Y,
            ArcColor.ToFColor(true), 2.f);
    }

    // ── 落點十字標記 ──
    const FVector2D& Landing = ScreenPts.Last();
    const float CR = 6.f;  // 十字半臂長
    const FLinearColor LandColor(1.f, 0.3f, 0.1f, 1.f);  // 橘紅色
    DrawLine(Landing.X - CR, Landing.Y, Landing.X + CR, Landing.Y, LandColor.ToFColor(true), 2.f);
    DrawLine(Landing.X, Landing.Y - CR, Landing.X, Landing.Y + CR, LandColor.ToFColor(true), 2.f);
}
```

### ARC-3：DrawHUD() 整合

**`ASkillCreatorHUD.cpp`** 的 `DrawHUD()` 末尾加：

```cpp
// ARC-3：投擲瞄準弧線（充電期間顯示）
DrawThrowArc();
```

---

## 四、技術細節說明

### 為什麼選 DrawLine 而非 Niagara Ribbon

| 方案 | Editor 操作 | 延遲 | 視覺效果 |
|------|------------|------|--------|
| Niagara Ribbon | 需建立 .uasset，設定 Ribbon Renderer | 低 | 光滑曲線，可有發光特效 |
| HUD DrawLine | 零 | 極低（純螢幕空間） | 折線，視覺乾淨但無 3D 深度感 |

初版用 DrawLine；若之後要升級 Niagara，計畫如下：
- 建立 NS_ThrowArc.uasset（Editor 一次性操作）
- `UNiagaraComponent` 跟隨角色，每幀更新 Ribbon 控制點
- 本計畫的弧線計算代碼可直接複用（計算邏輯不變，只換繪製方式）

### Gravity 方向（UE5 vs Godot）

- UE5：Z 向上，`GetGravityZ()` ≈ -980 cm/s²（向下 = Z 減小）
- 拋體公式：`pos(t) = start + v*t + 0.5*gravity*t²`，`gravity = (0, 0, GetGravityZ())`
- 不需翻轉，直接使用 `GetGravityZ()` 即可

### ChargePct = 0 時不畫

充電剛開始（指針在底部）時速度幾乎為零，弧線無意義。  
`if (ChargePct < 0.01f) return` 避免畫出接近垂直下落的假弧線。

### ProjectWorldLocationToScreen bPlayerViewportRelative

傳入 `true`，使投影座標相對於玩家 Viewport 而非整個 Editor Viewport，  
確保分割螢幕 / 多顯示器情境下座標正確。

---

## 五、可選擴充（ARC-opt）：點擊目標瞄準模式

如果日後想加「按住 F + 滑鼠點擊目標 → 自動計算拋射速度打到那點」：

```cpp
// 使用 UGameplayStatics::SuggestProjectileVelocity()
FVector OutVelocity;
const float MaxSpeed = BaseSpeed * Char->Stats.Strength / FMath::Max(0.1f, Mass);
if (UGameplayStatics::SuggestProjectileVelocity(
        GetWorld(), OutVelocity,
        LaunchPos, TargetWorldPos,
        MaxSpeed,
        false,           // bFavorHighArc
        0.f,             // CollisionRadius
        GetWorld()->GetGravityZ(),
        ESuggestProjVelocityTraceOption::DoNotTrace))
{
    // OutVelocity = 需要的初速度向量
    // 用 OutVelocity 替換 AimDir * Speed 繪製弧線
}
```

此模式需要：
- 射線偵測取得滑鼠世界座標（`PC->DeprojectScreenPositionToWorld()`）
- 解算投擲速度（`SuggestProjectileVelocity()`）
- 不需要改現有的充電 / 投擲系統（只影響 HUD 顯示）

---

## 六、Include 需求

`ASkillCreatorHUD.cpp` 需確認已有：
```cpp
#include "ASkillCreatorCharacter.h"
#include "APhysicalItemActor.h"
#include "ADebrisActor.h"
#include "AVoxelWorldActor.h"
#include "WorldScale.h"
#include "MaterialType.h"
#include "TileWorld3D.h"
```

若 `IPhysicalPickable` 相關 cast 還未 include，一起加入。

---

## 七、完成定義

- [ ] ARC-0：`UPhysicalThrowWidget::GetCurrentChargePct()` 宣告 + 實作（0 warning）
- [ ] ARC-1：`ASkillCreatorHUD::bIsChargingThrow` + `StartThrowCharge`/`FinishThrowCharge` 同步
- [ ] ARC-2：`DrawThrowArc()` — 拋體模擬 + 螢幕投影 + DrawLine + 落點十字
- [ ] ARC-3：`DrawHUD()` 末尾呼叫 `DrawThrowArc()`
- [ ] Build 通過 0 error 0 warning
- [ ] 遊戲驗證：持 F 充電時螢幕出現黃色拋物線弧線，力道越大弧線越長
