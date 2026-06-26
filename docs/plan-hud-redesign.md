# HUD 重設計實作計畫

> 設計來源：使用者附圖（2026-06-27 session）  
> 對齊問答摘要：HP 水缸由底往上填充、MP 等弧段逆時針、溫度條基準在底部正常時游標在中間、弧段平均等分、副手欄同現行實作只調位置、5 條垂直生存條

---

## 一、概述

重設計玩家 HUD 以符合設計圖，主要改動如下：

| # | 項目 | 性質 |
|---|------|------|
| HUD-1 | HP 圓形水缸 + MP 外環 | 全新 Widget |
| HUD-2 | 特殊狀態圖示區（泰拉瑞亞風格） | 全新 Widget |
| HUD-3 | 生存條改為 5 條垂直（移除心情/精力） | 改寫既有 |
| HUD-4 | 體溫雙向條（以中點為基準） | 改寫既有 |
| HUD-5 | 右上角小地圖（佔位圓形，無功能） | 全新 Widget |
| HUD-6 | 「拿取」物品槽（熱鍵欄右側圓形） | 全新 |
| HUD-7 | 副手欄位置調整 | 改座標 |
| HUD-8 | 心情從 HUD 移除（保留 PlayerPanel 中） | 刪除顯示 |
| HUD-9 | XP 條移至底部中央、加 LV 標示 | 改寫既有 |
| HUD-10 | 加工提示卡片位置調整 | 改座標 |

---

## 二、現行 HUD 佈局參考（改前）

```
左下角（bottom-left 錨點，座標 Y 向上為負）：
  - 副手欄:    X=10,  Y=-132, 48×48 圓形
  - 熱鍵欄:    X=66,  Y=-132, 10格×(48+4)=520px → 末格右邊=66+520+48=634px
  - 生存條:    X=10,  Y=-248 往上，水平條 6條+體溫文字
  - LV/XP:     X=10,  Y=-262~-290 區域
  - Mana VBox: X=10,  Y=-30 底往上 VBox
右上角：
  - 時鐘:      anchor(1,0) 右上角
```

---

## 三、各項詳細規格

---

### HUD-1：HP 圓形水缸 + MP 外環

**新建 `UHpMpCircleWidget`（`Source/SkillCreatorRuntime/Public/.h` + `Private/.cpp`）**

#### 視覺規格

| 參數 | 值 |
|------|----|
| HP 圓半徑 | 44 px（直徑 88）|
| MP 外環外半徑 | 56 px |
| MP 外環寬度 | 10 px |
| Widget 總尺寸 | 120×120 px（含外環＋邊距） |
| 位置 | 左上角，anchor(0,0)，offset(10, 10) |

#### 繪製策略（覆寫 `NativePaintWidget`）

**HP 水缸（水平切片法）：**  
對圓心 `(cx, cy)` 半徑 `R=44`，每條 2px 高的水平切片：  
```
chord_w(y) = 2 * sqrt(R² - (y - cy)²)
water_line = cy + R - 2R * HpPercent   // 水位線 Y（screen 向下）
若切片 y >= water_line → 繪製矩形 [cx - chord_w/2, y, chord_w, 2px]
```
顏色：正常時藍綠色 `(0.20, 0.75, 0.80)`，危急 HP < 30% 時橙紅 `(0.90, 0.30, 0.15)`

**圓圈外框：** `MakeLines`（24 段折線近似圓形），深色 `(0.08, 0.10, 0.12, 0.9)`

**HP 數字文字：** 圓心居中，白色，字號 11

**MP 外環（N 段等弧）：**  
設 N = 槽數（1~3），每段弧角 = 360°/N 度，弧起點角：從頂部（270°）順時針依槽編號排列。  
每段弧的填充範圍：從段起始角，**逆時針**延伸 `(Current/MaxCurrent) × (360°/N)` 度。  
用 `MakeLines`（每段 16 折線段）繪製，線寬 = MP 外環寬度。  
顏色：複用既有 `UPlayerHUDWidget::ManaColor(Key)`。  
空弧（MP=0 的部分）用灰暗底色 `(0.10, 0.10, 0.15, 0.5)` 繪製。

#### C++ 介面

```cpp
// 每幀由 ASkillCreatorHUD::TickHUD 呼叫：
void SetHpPercent(float Hp01);
void SetManaSlots(const TArray<FManaSlot>& Slots);
```

**`ASkillCreatorHUD` 變更：**  
移除 `HUDWidget->UpdateHpMp` 中對 `HpBar`/`MpBar` 的更新（保留 UpdateHpMp 簽章向後相容，但 HpBar/MpBar 欄位不再建立）；改呼叫 `HpMpCircle->SetHpPercent(HpPct)` 及 `HpMpCircle->SetManaSlots(Char->ActiveManaSlots)`。

---

### HUD-2：特殊狀態圖示區

**新建 `UAbnormalStatusBarWidget`（`Public/.h` + `Private/.cpp`）**

#### 視覺規格

| 參數 | 值 |
|------|----|
| 圖示大小 | 20×20 px |
| 圖示間距 | 3 px |
| 最大橫向數量 | 10（超過換行）|
| 位置 | 頂部中央，anchor(0.5, 0)，從螢幕頂部偏移 Y=8px |
| 對齊 | 水平置中 |

#### 圖示內容

每個圖示：
- 底色：依狀態類型（傷害類紅、移動封鎖橙、正面狀態藍綠）
- 文字：2~3 字縮寫（燒、毒、凍、麻、癱、暈……）
- 剩餘時間：圖示右下小字（秒數，若持續時間 ≤ 0 則不顯示）

#### 資料結構

```cpp
struct FAbnormalStatusInfo
{
    EAbnormalStatusId StatusId;
    FString ShortName;    // 2~3字縮寫
    FLinearColor Color;
    float RemainingSeconds;  // ≤0 = 永久
    int32 StackCount;        // >1 才顯示
};
```

#### C++ 介面

```cpp
void UpdateStatusBar(const TArray<FAbnormalStatusInfo>& Statuses);
// 每幀重建圖示列（ClearChildren + loop ConstructWidget），
// StatusId 組成不變時比對快取跳過重建
```

**`ASkillCreatorHUD` 變更：**  
在 `TickHUD` 呼叫 `StatusBar->UpdateStatusBar(BuildStatusInfoArray(Char->SpecialStatusComp))`。  
`BuildStatusInfoArray` 靜態輔助函式：掃描 `ActiveEffects`，組裝 `FAbnormalStatusInfo` 陣列。

**縮寫 / 顏色對照表（內建在 `.cpp` 靜態陣列）：**

| 狀態 | 縮寫 | 顏色 |
|------|------|------|
| 燒傷 | 燒 | 橙紅 `(0.95,0.35,0.10)` |
| 凍傷/結凍 | 凍 | 冰藍 `(0.35,0.70,1.00)` |
| 中毒 | 毒 | 毒綠 `(0.30,0.80,0.20)` |
| 麻痺/癱瘓 | 麻 | 黃 `(0.95,0.90,0.10)` |
| 暗蝕 | 暗 | 深紫 `(0.45,0.15,0.65)` |
| 流血 | 血 | 深紅 `(0.75,0.10,0.10)` |
| 暈眩 | 暈 | 亮黃 `(1.00,0.90,0.20)` |
| 石化 | 石 | 灰 `(0.55,0.55,0.50)` |
| 窒息 | 息 | 深藍 `(0.15,0.30,0.70)` |
| 恐懼 | 懼 | 暗棕 `(0.45,0.25,0.10)` |
| 噩運 | 凶 | 黑紫 `(0.30,0.10,0.35)` |
| 無敵 | 無 | 金黃 `(1.00,0.85,0.15)` |
| 霸體 | 霸 | 橙 `(0.95,0.55,0.10)` |
| 虛化 | 虛 | 白藍 `(0.70,0.85,1.00)` |

---

### HUD-3：生存條改垂直 5 條

**改寫 `UPlayerHUDWidget::BuildSurvivalBars`**

#### 視覺規格

| 參數 | 值 |
|------|----|
| 條寬 | 10 px |
| 條高 | 80 px |
| 條間距 | 8 px |
| 起始位置 | 左下角，X=10，Y=-100（從 bottom-left 往上量） |
| 排列 | 5 條橫排，從左到右：飢餓 / 口渴 / 氧氣 / 體力 / 溫度 |
| 標籤 | 條正下方，字號 9，顏色淡灰 |

#### 顏色

| 條 | 顏色 |
|----|------|
| 飢餓 | `(0.60, 0.88, 0.22)` |
| 口渴 | `(0.28, 0.78, 1.00)` |
| 氧氣 | `(0.45, 0.65, 1.00)` |
| 體力 | `(1.00, 0.60, 0.22)` |
| 溫度 | 冷時藍 `(0.30, 0.55, 1.00)`；熱時橙紅 `(0.95, 0.40, 0.15)`；正常灰白 |

#### 移除項目

- 移除「心情」條（Mood）的顯示——`SurvivalBarFills[2]` 不再建立
- 移除「精力」條（MentalEnergy）的顯示
- 保留 `UpdateSurvival` 中 `Mood` 和 `MentalEnergy` 的讀取（但只顯示 5 條，不向 widget 更新）
- `BodyTempLabel`（純文字）改為 HUD-4 的雙向條實作取代

#### Widget 成員欄位

刪除 `SurvivalBarFills`/`SurvivalValLabels` 的 TArray；改用 5 個具名欄位：
```cpp
TObjectPtr<UBorder> HungerFill, ThirstFill, OxygenFill, StaminaFill;
// 溫度由 HUD-4 的雙向條成員取代 BodyTempLabel
```
（或維持 TArray，索引 0~3 對應 飢餓/口渴/氧氣/體力，index 4 為溫度 placeholder）

---

### HUD-4：體溫雙向條

**改寫體溫顯示（整合進 `BuildSurvivalBars` 最後一格）**

#### 原理

```
正常體溫 NormalBodyTemp = 36.5°C
冷極 = HypothermiaThreshold = 10.0°C → ColdHalfRange = 36.5 - 10.0 = 26.5°C
熱極 = HeatstrokeThreshold  = 42.0°C → HotHalfRange  = 42.0 - 36.5 =  5.5°C

HotFraction  = clamp((T - Normal) / HotHalfRange,  0, 1)
ColdFraction = clamp((Normal - T) / ColdHalfRange, 0, 1)
```

#### 視覺結構（同一條 80px 高、10px 寬）

```
┌──────┐  ← Y_top  (熱極)
│ 熱填 │  橙紅，高度 = HotFraction  × 40px，從中線往上
├──────┤  ← Y_mid  (中線，正常體溫標記，1px 白線)
│ 冷填 │  藍色，高度 = ColdFraction × 40px，從中線往下
└──────┘  ← Y_bot  (冷極)
```

背景為暗灰色（整條高度），Hot Fill 和 Cold Fill 分別用 Canvas 內的兩個 UBorder 實作，每幀更新 `SetSize` 高度。

#### Widget 成員

```cpp
TObjectPtr<UBorder> TempBarBg;
TObjectPtr<UBorder> TempHotFill;   // from mid up
TObjectPtr<UBorder> TempColdFill;  // from mid down
```

#### UpdateSurvival 計算

```cpp
constexpr float Normal    = UCharacterStateComponent::NormalBodyTemp;
constexpr float ColdRange = Normal - UCharacterStateComponent::HypothermiaThreshold;
constexpr float HotRange  = UCharacterStateComponent::HeatstrokeThreshold - Normal;
const float T = S->BodyTemperature;
float HotF  = FMath::Clamp((T - Normal) / HotRange,  0.f, 1.f);
float ColdF = FMath::Clamp((Normal - T) / ColdRange, 0.f, 1.f);
// 更新 TempHotFill 高度（往上）和 TempColdFill 高度（往下）
```

---

### HUD-5：小地圖佔位圓形

**新建 `UMinimapPlaceholderWidget`（或直接在 `BuildClock` 函式後加入 `BuildMinimap`）**

| 參數 | 值 |
|------|----|
| 直徑 | 80 px |
| 位置 | 右上角，anchor(1, 0)，X=-90, Y=10 |
| 外觀 | 藍灰半透明圓形（RoundedBox 全圓角），內部置中文字「地圖」|
| 功能 | 無（UI 佔位） |

**推薦做法：** 加入 `UPlayerHUDWidget::BuildMinimap(UCanvasPanel* Root)` 在 `BuildClock` 旁，不建立新 Widget 類別。

---

### HUD-6：「拿取」物品槽

**新增 `BuildCarrySlot(UCanvasPanel* Root)` 及 `UpdateCarrySlot` API**

| 參數 | 值 |
|------|----|
| 尺寸 | 48×48 px 圓形（對齊副手欄） |
| 位置 | 熱鍵欄右側再往右 8px：X = 66 + 10×(48+4) + 8 = 594, Y=-132 |
| 外框 | 同副手欄樣式，RoundedBox 全圓角，藍灰邊框（非啟用態） |
| 內容 | 拿取中顯示 ItemId 對應的顏色色塊（複用 `ItemIconColor(Id)`）；空時暗灰 |

#### C++ 欄位

```cpp
TObjectPtr<UBorder>    CarryBorder    = nullptr;
TObjectPtr<UBorder>    CarryIconBorder = nullptr;
TObjectPtr<UTextBlock> CarryKeyLabel  = nullptr;  // 可選提示鍵（E）
```

#### C++ 介面

```cpp
void UpdateCarrySlot(bool bIsCarrying, EItemId ItemId = EItemId::None);
```

**`ASkillCreatorHUD::TickHUD` 變更：**

```cpp
const bool bCarrying = Char->IsCarrying();
const EItemId CarryId = bCarrying
    ? Cast<APhysicalItemActor>(Char->CarriedActor.Get())
      ? Cast<APhysicalItemActor>(Char->CarriedActor.Get())->GetInventoryItemId()
      : EItemId::None
    : EItemId::None;
HUDWidget->UpdateCarrySlot(bCarrying, CarryId);
```

---

### HUD-7：副手欄位置調整

**改 `BuildOffhandSlot` 座標（若設計圖有具體位置需求）**

目前：`X=10, Y=-132`（距熱鍵欄左側 8px 間距）  
新位置根據設計圖（熱鍵欄正左側、底部對齊）：維持現有邏輯不動，
但若整體熱鍵欄中心化，需同步調整 StartX。

> **待確認**：設計圖中副手欄是否隨熱鍵欄整體中移，還是固定在左側？  
> 預設：維持現行 bottom-left 對齊，熱鍵欄不中移。

---

### HUD-8：心情從 HUD 移除

**改 `UPlayerHUDWidget`：**

- `UPROPERTY(meta=(BindWidgetOptional)) MoodBar`：保留定義（向後相容 BP），不在 `NativeOnInitialized` 中建立對應的 C++ 元素
- `UpdateHpMp(float Hp, float MaxHp, float Mp, float MaxMp, float StaminaPct, float MoodPct)` 簽章**維持不變**（ASkillCreatorHUD 仍傳入 MoodPct，但 widget 不使用）
- 刪除 `MoodPercent` 欄位的任何 UI 更新（`if (MoodBar) MoodBar->SetPercent(...)` 移除）

**確認 `UPlayerPanelWidget` Stats Tab 保留 Mood 顯示：**  
grep `Mood` in `UPlayerPanelWidget.cpp`，確認有 `Mood` 條/數值欄位。若無，在 Stats Tab 加一行「心情：XX」文字。

---

### HUD-9：XP 條移至底部中央

**改 `BuildLevelHud`：**

| 改動 | 舊 | 新 |
|------|----|----|
| XP 條 X 起點 | 10 | (ViewportW/2) - XpBarMaxWidth/2（anchor改 center-bottom） |
| XP 條寬度 | 200 px | 520 px（對齊熱鍵欄總寬 10×48+9×4=516 → 520） |
| XP 條 Y | -262 | -100（底部，高度 10px） |
| LV 標籤 | 條左上方獨立 | 條內居中（anchor 1/2, 1/2），黑色或白色字 |
| 境界 (Tier) 標籤 | LV 右側 | 移至 XP 條右側外（較小字） |
| XP 數量文字 | 條左上 | 移至 XP 條右側外（字號減小，灰色） |

改用 `Pin(XpBg, {-260, -100}, {520, 10}, FAnchors(0.5f, 1.f, 0.5f, 1.f), {0.5f, 1.f})` 水平置中。

---

### HUD-10：加工提示卡片位置

**改 `UCraftingHintCardWidget::NativeOnInitialized`：**

目前：anchor(0, 0.5)，X=10，Y=-H/2（螢幕正中高度左側）  
新位置：依設計圖，應在左側中高區域，Y 位置需貼近 HP 圓形下方。

建議新 anchor：`(0, 0.5)`，offset `(10, 50)`（相對 anchor 中心向下偏移，讓卡片出現在螢幕左側偏上方而非正中央）。

> **待確認**：需看設計圖中「展開/可加工提示」的確切位置後調整。

---

## 四、實作順序

無相依的可並行實作，建議順序：

```
Phase 1（低風險，改既有座標/刪除）
  HUD-8  心情 HUD 移除        → 只刪 MoodBar 更新
  HUD-7  副手欄位置確認        → 可能不需改動
  HUD-10 加工卡片位置          → 改一個座標

Phase 2（重構既有）
  HUD-3  垂直生存條            → 重寫 BuildSurvivalBars
  HUD-4  體溫雙向條            → 連帶 HUD-3 同時完成
  HUD-9  XP 條置中            → 重寫 BuildLevelHud anchor

Phase 3（新增 Widget/元素）
  HUD-5  小地圖佔位            → 加 BuildMinimap
  HUD-6  拿取槽               → 加 BuildCarrySlot + UpdateCarrySlot

Phase 4（最複雜，最後）
  HUD-1  HP 圓形水缸 + MP 外環 → 新 UHpMpCircleWidget
  HUD-2  特殊狀態圖示區        → 新 UAbnormalStatusBarWidget
```

---

## 五、需要 Rebuild 的檔案

所有新增 `UCLASS` 均需關 Editor + 完整 Rebuild（`.h` 有 UClass 宣告）：
- `UHpMpCircleWidget.h`（新建）
- `UAbnormalStatusBarWidget.h`（新建）
- `UPlayerHUDWidget.h`（增加新欄位 `CarryBorder`、`TempHotFill` 等）

`.cpp` 只改邏輯（`BuildSurvivalBars`、`BuildLevelHud`、`BuildOffhandSlot`）：可 Live Coding。

---

## 六、驗證清單

- [ ] HP 圓形水位在 HP 變化時平滑升降（無鋸齒邊緣）
- [ ] MP 外環：單 MP 類型時完整環；雙 MP 類型時兩等弧；三 MP 類型時三等弧
- [ ] MP 外環逆時針消耗（消耗後弧縮短）
- [ ] 特殊狀態：燒傷圖示在施加後立即出現，移除後消失
- [ ] 5 條垂直生存條正確映射飢餓/口渴/氧氣/體力
- [ ] 體溫條：36.5°C 時游標在中間（無填充）；> 42°C 時上方橙紅滿填；< 10°C 時下方藍色滿填
- [ ] 拿取槽：BeginCarry 後圖示顯示，EndCarry 後清空
- [ ] XP 條置中對齊熱鍵欄
- [ ] 心情不在主 HUD 顯示，但 PlayerPanel Stats 仍有數值
- [ ] Build 0 錯誤 0 警告
