# 遊戲內時間系統（GameClock）實作計畫

> 撰寫日期：2026-06-26
> 狀態：✅ 完成（2026-06-26 Build 0 錯誤）

---

## 一、現況確認

### Godot `GameClock.cs`

極簡版，僅有：

| 項目 | 內容 |
|------|------|
| `TotalTicks` | 自本局累積 tick 數 |
| `DayCount` | 已過天數 |
| `DayFraction` | 當天進度 (0.0–1.0) |
| `Advance(delta)` | 每幀推進 |
| `Reset()` | 場景重啟時清零 |

實際用途只有兩處：SpellRunner 的 `SubmittedAt` 時間戳、SnapshotManager 的 `AnchorTimestamp`。  
**無曆法、無顯示、無存讀、無暫停機制。**

### UE5 `UGameClockSubsystem`（現況）

Godot 的直接復刻，多了 `DayHour`：

- ✅ 已在 `ASkillCreatorCharacter::Tick()` 每幀驅動
- ✅ SpellRunner / Snapshot 的 `TotalTicks` 時間戳已接通
- ❌ `BeginPlay()` 每次進遊戲都 `Reset()` → TotalTicks 歸零，無法跨 session 累積
- ❌ 無曆法（年/月/日/時/分/星期）
- ❌ 無存讀（未寫入 `FWorldSaveData`）
- ❌ 無暫停控制（離開世界時不停止）
- ❌ 無晝夜邊界定義
- ❌ 無 HUD 顯示

**結論**：曆法 + 顯示 + 存讀 + 暫停控制全部是新需求，非 Godot 復刻，為全新設計。

---

## 二、設計規格

### 2-1 起始時間

世界首次進入時，遊戲內時間設定為：

```
蒼和 5000 年  1 月 1 日  00：00  星期一
```

### 2-2 時間比例

| 遊戲單位 | 現實時間 | 說明 |
|---------|---------|------|
| 1 遊戲分鐘 | 1 秒 | 分鐘數字每秒跳動一次 |
| 1 遊戲小時 | 60 秒（1 現實分鐘） | 小時數字每 60 秒跳動一次 |
| 1 遊戲日 | 1440 秒（24 現實分鐘） | 與現有 TicksPerDay=28800 完全吻合 |

現有 Tick 常數驗算：

```
TicksPerSecond = 20
TicksPerDay    = 28800
→ 1 遊戲日 = 28800 / 20 = 1440 現實秒 = 24 現實分鐘 ✓
→ 1 遊戲小時 = 1440 / 24 = 60 現實秒 ✓
→ 1 遊戲分鐘 = 60 / 60 = 1 現實秒 ✓
```

新增衍生常數：

```
TicksPerMinute = 20   (= TicksPerSecond)
TicksPerHour   = 1200 (= 60 × 20)
```

### 2-3 曆法規則

| 規則 | 數值 |
|------|------|
| 每週天數 | 7 天（星期一～日） |
| 每月天數 | 30 天 |
| 每年月數 | 12 個月 |
| 每年天數 | **360 天**（12 × 30，設計上 30 天/月為整除基準） |

> 注意：「每 365 天為一年」與「每 30 天為一個月」在整除上無法兼容（12×30=360≠365）。
> 本計畫採 **360 天/年**。若後續世界觀確定使用 365 天年，需調整月份演算法（例如月 12 改為 35 天）。

### 2-4 時間流動規則

- 只有玩家**正在遊戲中**（非標題畫面、非暫停）時，時間流動。
- 玩家離開世界 / 回到主選單 → 時間暫停。
- 遊戲暫停（`PC->SetPause(true)`）→ 時間自然停止（Tick 不執行）。
- 未來多人世界另行處理，目前不考慮。

### 2-5 晝夜定義

| 時段 | 時間 |
|------|------|
| 白天開始（Dawn） | 上午 04：30 |
| 夜晚開始（Dusk） | 下午 19：30 |

對應 Tick 偏移（自當天 00:00 起算）：

```
DawnTickOfDay = (4 × 60 + 30) × 20 = 5400
DuskTickOfDay = (19 × 60 + 30) × 20 = 23400
```

### 2-6 HUD 顯示格式

位置：遊戲畫面**右側**，固定顯示於遊戲中。

```
蒼和XXXX年 XX月XX日 XX：XX 星期X
```

範例：`蒼和5000年 01月01日 00：00 星期一`

---

## 三、需修改的檔案清單

| 步驟 | 檔案 | 類型 | 備註 |
|------|------|------|------|
| T-1 | `Source/SkillCreatorRuntime/Public/UGameClockSubsystem.h` | `.h` UPROPERTY | ⚠️ 需 Rebuild |
| T-1 | `Source/SkillCreatorRuntime/Private/UGameClockSubsystem.cpp` | `.cpp` | — |
| T-2 | `Plugins/VoxelWorld/Source/VoxelWorld/Public/WorldSaveData.h` | plain struct `.h` | 需重新編譯 |
| T-2 | `Plugins/VoxelWorld/Source/VoxelWorld/Private/WorldSaveData.cpp` | `.cpp` | — |
| T-3 | `Source/SkillCreatorRuntime/Private/ASkillCreatorGameMode.cpp` | `.cpp` only | 可 Live Coding |
| T-3 | `Source/SkillCreatorRuntime/Private/ASkillCreatorCharacter.cpp` | `.cpp` only | 可 Live Coding |
| T-4 | `Source/SkillCreatorRuntime/Public/UPlayerHUDWidget.h` | `.h` UPROPERTY | ⚠️ 需 Rebuild |
| T-4 | `Source/SkillCreatorRuntime/Private/UPlayerHUDWidget.cpp` | `.cpp` | — |

**→ 四個步驟一次性關 Editor 完整 Rebuild。**

---

## 四、T-1｜UGameClockSubsystem 曆法擴充

### `.h` 新增

```cpp
// ── 曆法常數 ──────────────────────────────────
static constexpr int32 StartYear      = 5000;
static constexpr int32 DaysPerMonth   = 30;
static constexpr int32 MonthsPerYear  = 12;
static constexpr int32 DaysPerYear    = 360;   // 12 × 30
static constexpr int32 TicksPerMinute = 20;    // = TicksPerSecond
static constexpr int32 TicksPerHour   = 1200;  // 60 × 20
static constexpr int32 DawnTickOfDay  = 5400;  // 04:30
static constexpr int32 DuskTickOfDay  = 23400; // 19:30

// ── 狀態 ──────────────────────────────────────
UPROPERTY(BlueprintReadOnly, Category="GameClock")
bool bClockPaused = false;

// ── 新增方法 ──────────────────────────────────
UFUNCTION(BlueprintPure, Category="GameClock")
FGameDateTime GetDateTime() const;

UFUNCTION(BlueprintPure, Category="GameClock")
FString GetFormattedTime() const;

UFUNCTION(BlueprintPure, Category="GameClock")
bool IsDay() const;

void SetTicks(int64 Ticks);
void PauseClock();
void ResumeClock();
```

新增輔助結構（宣告在同一 `.h`，需放在 class 前）：

```cpp
USTRUCT(BlueprintType)
struct FGameDateTime
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 Year      = 5000;
    UPROPERTY(BlueprintReadOnly) int32 Month     = 1;
    UPROPERTY(BlueprintReadOnly) int32 Day       = 1;
    UPROPERTY(BlueprintReadOnly) int32 Hour      = 0;
    UPROPERTY(BlueprintReadOnly) int32 Minute    = 0;
    UPROPERTY(BlueprintReadOnly) int32 DayOfWeek = 0; // 0=一, 1=二…6=日
};
```

### `.cpp` 演算法

**`GetDateTime()`：**
```
totalDays   = TotalTicks / TicksPerDay
tickOfDay   = TotalTicks % TicksPerDay
minuteOfDay = tickOfDay / TicksPerMinute

Year      = StartYear + totalDays / DaysPerYear
dayInYear = totalDays % DaysPerYear         // 0..359
Month     = dayInYear / DaysPerMonth + 1    // 1..12
Day       = dayInYear % DaysPerMonth + 1    // 1..30
Hour      = minuteOfDay / 60               // 0..23
Minute    = minuteOfDay % 60               // 0..59
DayOfWeek = totalDays % 7                  // 0=星期一（起始日即星期一）
```

**`GetFormattedTime()`：**
```cpp
static const FString DayNames[] = {TEXT("一"),TEXT("二"),TEXT("三"),
                                    TEXT("四"),TEXT("五"),TEXT("六"),TEXT("日")};
FGameDateTime DT = GetDateTime();
return FString::Printf(TEXT("蒼和%d年 %02d月%02d日 %02d：%02d 星期%s"),
    DT.Year, DT.Month, DT.Day, DT.Hour, DT.Minute, *DayNames[DT.DayOfWeek]);
```

**`IsDay()`：**
```cpp
int64 TickOfDay = TotalTicks % TicksPerDay;
return TickOfDay >= DawnTickOfDay && TickOfDay < DuskTickOfDay;
```

**`Advance()`** 加暫停守衛：
```cpp
void UGameClockSubsystem::Advance(float DeltaSeconds)
{
    if (bClockPaused) return;   // ← 新增
    // 以下不變
    Accumulator += DeltaSeconds * (float)TicksPerSecond;
    ...
}
```

**`SetTicks()` / `PauseClock()` / `ResumeClock()`：**
```cpp
void UGameClockSubsystem::SetTicks(int64 Ticks) { TotalTicks = Ticks; Accumulator = 0.f; }
void UGameClockSubsystem::PauseClock()           { bClockPaused = true; }
void UGameClockSubsystem::ResumeClock()          { bClockPaused = false; }
```

**移除**：`DayCount` UPROPERTY（改由 `GetDateTime()` 的 `totalDays` 衍生；SpellRunner / Snapshot 只用 `TotalTicks`，不受影響）。

---

## 五、T-2｜FWorldSaveData 新增 ElapsedTicks

### `WorldSaveData.h`

```cpp
int64 ElapsedTicks = 0;   // 本世界累積 tick 數，進入時載入到 Clock
```

### `WorldSaveData.cpp`（`SaveMeta` / `LoadMeta`）

在既有 key=value 序列化裡加入：

```
// SaveMeta
Lines.Add(FString::Printf(TEXT("ElapsedTicks=%lld"), ElapsedTicks));

// LoadMeta
if (Key == TEXT("ElapsedTicks")) Out.ElapsedTicks = FCString::Atoi64(*Value);
```

---

## 六、T-3｜GameMode + Character 接線

### `ASkillCreatorGameMode::StartGameplayWithWorld()`

進入世界時載入時鐘（加在 `VW` / 角色 Spawn 之前）：

```cpp
if (auto* GI = GetGameInstance())
if (auto* Clock = GI->GetSubsystem<UGameClockSubsystem>())
{
    Clock->SetTicks(World.ElapsedTicks);   // 新世界=0，舊世界=存檔值
    Clock->ResumeClock();
}
```

### `ASkillCreatorGameMode::FillSaveData()`

SaveAll 呼叫前，把當前 Tick 寫回 WorldSaveData：

```cpp
if (auto* GI = GetGameInstance())
if (auto* Clock = GI->GetSubsystem<UGameClockSubsystem>())
    CurrentWorldSave.ElapsedTicks = Clock->TotalTicks;
```

### `ASkillCreatorCharacter::BeginPlay()`

**刪除** 目前第 115–117 行的 `Clock->Reset()` 呼叫。

> 原因：GameMode 的 `StartGameplayWithWorld()` 已在 Spawn 角色前呼叫 `SetTicks()`，
> BeginPlay 的 Reset 會把 GameMode 已設好的值歸零。

### 離開世界時

在返回標題畫面的路徑（目前 `UGameFlowWidget` 的返回邏輯）加：

```cpp
Clock->PauseClock();
```

> 角色 Tick 消失後 Advance 自然不再執行，PauseClock 作為額外保險。

---

## 七、T-4｜UPlayerHUDWidget 時鐘顯示

### `UPlayerHUDWidget.h` 新增成員

```cpp
TObjectPtr<UTextBlock> ClockText = nullptr;
```

### `UPlayerHUDWidget.cpp`

在 `BuildLayout()` / `NativeOnInitialized()` 裡加入 `BuildClock()`：

```cpp
void UPlayerHUDWidget::BuildClock()
{
    // 建立右上角 CanvasPanel Slot
    UCanvasPanel* Root = ...;   // 取得既有 root canvas
    ClockText = NewObject<UTextBlock>(this);
    ClockText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    // FontSize 設約 14，FontFamily 依專案字型

    UCanvasPanelSlot* Slot = Root->AddChildToCanvas(ClockText);
    Slot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));   // 右上
    Slot->SetAlignment(FVector2D(1.f, 0.f));
    Slot->SetPosition(FVector2D(-12.f, 12.f));          // 距右邊緣 12px
    Slot->SetAutoSize(true);
}
```

`NativeTick()` 更新（只在分鐘邊界時重設文字，避免每幀 FString 分配）：

```cpp
void UPlayerHUDWidget::NativeTick(const FGeometry& Geo, float DeltaTime)
{
    Super::NativeTick(Geo, DeltaTime);
    // ... 其他現有更新 ...

    // 時鐘更新（每遊戲分鐘邊界觸發，即每現實秒一次）
    if (ClockText)
    {
        if (auto* GI = GetWorld()->GetGameInstance())
        if (auto* Clock = GI->GetSubsystem<UGameClockSubsystem>())
        {
            int64 CurMin = Clock->TotalTicks / UGameClockSubsystem::TicksPerMinute;
            if (CurMin != LastClockMinute)
            {
                LastClockMinute = CurMin;
                ClockText->SetText(FText::FromString(Clock->GetFormattedTime()));
            }
        }
    }
}
```

並在 `.h` 加 `int64 LastClockMinute = -1;`（私有成員，不需 UPROPERTY）。

---

## 八、批次 Rebuild 順序

```
1. 關閉 UE Editor（含 Live Coding）
2. 完成 T-1 (.h/.cpp) + T-2 (.h/.cpp) + T-4 (.h/.cpp) 所有檔案改動
3. T-3 的 .cpp 改動一起做（不需 UHT，同批次 Rebuild 更省時）
4. 執行完整 Rebuild（UBT）
5. 打包 Standalone
```

---

## 九、驗收測試

| # | 測試項目 | 預期結果 |
|---|---------|---------|
| V-1 | 進新世界 | 時鐘顯示「蒼和5000年 01月01日 00：00 星期一」 |
| V-2 | 等待 1 現實秒 | 分鐘 +1（00：01） |
| V-3 | 等待 60 現實秒 | 小時 +1（01：00） |
| V-4 | 等待至隔天（24 現實分鐘） | 日期 +1、星期 +1 |
| V-5 | 離開世界 → 重進同一世界 | 時鐘從離開時的值繼續，不重置 |
| V-6 | 進入新世界 | 時鐘重新從 蒼和5000年 1月1日 00:00 開始 |
| V-7 | 遊戲暫停 | 時鐘停止（Tick 不執行） |
| V-8 | 04:29 → 04:30 | `IsDay()` 由 false 切換為 true |
| V-9 | 19:29 → 19:30 | `IsDay()` 由 true 切換為 false |
| V-10 | 滿 30 天 | 月份 +1、日期回到 01 |
| V-11 | 滿 360 天 | 年份 +1 |
