# 玩家個人面板重整計畫

> 建立日期：2026-06-25
> 狀態：計畫草案，待確認後開始實作
> 觸發需求：G/B/V/R/T 五個面板鍵整合，UX 重新設計

---

## 一、設計概述

### 1-A 整合目標

| 舊鍵 | 舊功能 | 新對應 |
|------|--------|--------|
| G    | 個人屬性面板（UStatsWidget）| G → 開 `UPlayerPanelWidget`（整合入 Tab 1） |
| V    | 技能創建空間（USpellListWidget）| 整合入 `UPlayerPanelWidget` Tab 3 |
| B    | 設定（USettingsWidget）| 整合入 `UPlayerPanelWidget` 右上「設定」按鈕 |
| R    | 物品欄（UInventoryWidget）| R → 物品欄 + 裝備欄同時開關 |
| T    | 裝備欄（UEquipmentWidget）| 併入 R，T 不再有預設鍵位 |

整合後 **V / B / T 均無預設鍵位**，鍵位空出供未來使用。

---

### 1-B 玩家個人面板（G 鍵）視覺佈局

```
┌────────────────────────────────────────────────────────────────┐
│  個人資訊面板   職業能力   技能創建空間   心象空間       ●引導 ●圖鑑 ●設定  │
│  ─────────                                                       │
│                                                                  │
│                     [Tab 內容區]                                  │
│                                                                  │
└────────────────────────────────────────────────────────────────┘
```

- 上方 4 個文字頁籤（下底線高亮當前選中），點擊切換 Tab 內容區
- 右上角 3 個圓形按鈕：引導 / 圖鑑 / 設定
  - 引導、圖鑑：點擊後**切換整個畫面**至對應全版介面，左上角顯示返回鍵 + 標題（此版本先用空白佔位，導覽模式同舊版技能編輯頁面）
  - 設定：點擊後切換至 `UPlayerSettingsWidget` 全版（同上模式）
- 整個面板預設關閉；按 G 開關；切換至引導/圖鑑/設定後，G 鍵關閉的是當前顯示的子頁（不是直接跳回遊戲）

---

### 1-C 設定子面板 (UPlayerSettingsWidget)

```
┌─────────────────────────────────┐
│ 設定                        [X] │
├─────────────────────────────────┤
│ ◎ 操作細節                       │
│   □ 長按連放                      │
│   □ 完美移除放置                   │
├─────────────────────────────────┤
│ ◎ 鍵位設定                       │
│ ┌──────────────┬──────────────┐ │
│ │ 動作名稱     │ 目前鍵位     │ │
│ ├──────────────┼──────────────┤ │
│ │ 向前移動     │ W            │ │
│ │ 向後移動     │ S            │ │
│ │ ...          │ ...          │ │
│ └──────────────┴──────────────┘ │
│ 點擊鍵位欄 → 黃框等待輸入          │
│ 同時按下 1~4 個鍵 → 更新綁定       │
└─────────────────────────────────┘
```

鍵位設定行為：
- 捲動表格列出所有已定義動作與目前鍵位
- 點擊某行的鍵位欄 → 黃色邊框高亮（「待輸入」狀態）
- 此時玩家可同時按下 1~4 個鍵 → 偵測到所有鍵都鬆開後 → 儲存新綁定
- 鍵位設定**跟隨遊戲存檔（Game Save），不跟隨角色** → 即換角色也保留
- 提供「還原預設」按鈕

---

### 1-D R 鍵：物品欄 + 裝備欄合併開關

- 按一次 R → 物品欄（UInventoryWidget）+ 裝備欄（UEquipmentWidget）同時顯示
- 再按一次 R → 兩者同時關閉
- 視覺上並排顯示（物品欄左，裝備欄右）或分上下，具體位置沿用現有面板座標，兩者 ZOrder 相同

---

## 二、新增與修改的元件

### 2-A 新建 Widget

| 類別名稱 | 說明 |
|---------|------|
| `UPlayerPanelWidget` | G 鍵主面板（4 Tab + 3 圓形按鈕框架） |
| `UOccupationWidget` | 職業能力 Tab（此版本空白佔位） |
| `UInnerWorldWidget` | 內部空間 Tab（此版本空白佔位） |
| `UGuideWidget` | 引導全版介面（左上角返回鍵，此版本空白佔位） |
| `UCompendiumWidget` | 圖鑑全版介面（左上角返回鍵，此版本空白佔位） |
| `UPlayerSettingsWidget` | 設定子面板（鍵位設定 + 操作細節） |

### 2-B 修改 Widget

| 類別名稱 | 修改方向 |
|---------|---------|
| `UInputSettingsWidget` | 加入「點擊行 → 黃框等待」→「多鍵組合偵測」UI；`FKeyBindingEntry` 改為支援 `TArray<FKey>` |
| `ASkillCreatorHUD` | 加入 `UPlayerPanelWidget` 成員；移除獨立 `StatsPanel` / `SpellListPanel`（改由 `PlayerPanelWidget` 內部持有）；`ToggleInventory` / `ToggleEquipment` 改為 `ToggleInventoryAndEquipment()` 同時操作 |
| `ASkillCreatorPlayerController` | 鍵位重整（見 §三） |

### 2-C 廢棄 Widget

| 類別名稱 | 處理方式 |
|---------|---------|
| `USettingsWidget` | 功能移至 `UPlayerSettingsWidget`；原類別可整個刪除 |

---

## 三、鍵位重整（ASkillCreatorPlayerController）

| 鍵 | 目前綁定 | 新綁定 |
|----|---------|--------|
| G  | `OnOpenStats` → `ToggleStats()` | `OnOpenPlayerPanel` → `TogglePlayerPanel()` |
| R  | `OnOpenInventory` → `ToggleInventory()` | `OnOpenInventoryAndEquipment` → `ToggleInventoryAndEquipment()` |
| V  | `OnOpenEditor` → `ToggleSpellList()` | **移除**（空出此鍵） |
| B  | `OnOpenSettings` → `ToggleSettings()` | **移除**（空出此鍵） |
| T  | `OnOpenEquipment` → `ToggleEquipment()` | **移除**（空出此鍵） |

`ASkillCreatorHUD` 對應清理：
- 移除 `ToggleStats()` 的公開 API（或保留但不被鍵位綁定）
- 移除 `ToggleSettings()`（由面板內按鈕觸發）
- 移除 `ToggleSpellList()`（由 Tab 切換觸發，不需要從外部開關）
- 新增 `TogglePlayerPanel()`
- 新增 `ToggleInventoryAndEquipment()`

---

## 四、UInputSettingsWidget 多鍵組合改造

### 4-A 資料結構變更

```cpp
// 舊
UPROPERTY(BlueprintReadOnly) FKey BoundKey;

// 新
UPROPERTY(BlueprintReadOnly) TArray<FKey> BoundKeys; // 最多 4 個
```

### 4-B UI 狀態機（點擊 → 等待輸入 → 儲存）

```
Normal     → [點擊該行鍵位欄]    → Awaiting
Awaiting   → [偵測到按鍵]        → 累積 KeysDown（最多 4 個）
Awaiting   → [所有鍵都鬆開 or 0.5s timeout] → 寫入新綁定 → Normal
Awaiting   → [按 Escape]         → 取消 → Normal
```

偵測手段：
- 在 `NativeOnKeyDown` 累積按下的鍵
- 在 `NativeOnKeyUp`：若 `KeysDown.Num() > 0`，代表有鍵還沒全部鬆開；全鬆開時 commit

### 4-C 儲存位置

現有 `ConfigFilePath()` 回傳的路徑（GameSaved/Config 下的自訂 .ini）已是跟遊戲存檔同目錄，
沿用既有機制即可。若日後改為存進 USaveGame 物件，也只需更換 `SaveBindings()` 實作。

### 4-D 表格 UI

- `UScrollBox` 包住 N 行 `UHorizontalBox`：
  - 左欄 `UTextBlock`（動作名稱，例如「向前移動」）
  - 右欄 `UBorder` + `UTextBlock`（目前鍵位，例如「W」或「Ctrl + Shift + W」）
- 點擊右欄 `UBorder` 時：設 `PendingRebindRow`，邊框顏色改黃色
- 呼叫 `FSlateApplication::Get().SetKeyboardFocus(this)` 確保 KeyDown 路由到此 Widget

---

## 五、ASkillCreatorHUD 重整

### 5-A 成員變動

```cpp
// 移除
TObjectPtr<UStatsWidget>        StatsPanel;       // 移入 UPlayerPanelWidget 內部
TObjectPtr<USpellListWidget>    SpellListPanel;   // 移入 UPlayerPanelWidget 內部
TObjectPtr<USettingsWidget>     SettingsPanel;    // 廢棄，功能改由 UPlayerSettingsWidget 處理

// 新增
TObjectPtr<UPlayerPanelWidget>  PlayerPanel;
```

`InventoryPanel` 和 `EquipmentPanel` 繼續保留在 HUD，只是開關邏輯改為同步。

### 5-B API 變動

```cpp
// 移除（從外部可見的鍵位綁定觀點）
void ToggleStats();
void ToggleSpellList();
void ToggleSettings();
void ToggleEquipment();
void ToggleInventory();

// 新增
void TogglePlayerPanel();                 // G 鍵觸發
void ToggleInventoryAndEquipment();       // R 鍵觸發：兩者同時開關
```

---

## 六、UPlayerPanelWidget 實作細節

### 6-A Tab 切換

```cpp
// 4 個 Tab 按鈕 + 1 個 content 切換區
enum class EPlayerTab { Stats = 0, Occupation = 1, SpellEditor = 2, InnerWorld = 3 };

void SwitchTab(EPlayerTab Tab);
// → 隱藏所有 Tab content → 顯示對應 content → 更新 Tab 底線高亮
```

### 6-B 子 Widget 持有方式

```cpp
// 在 NativeOnInitialized() 建立（不是 NativeConstruct()）
TObjectPtr<UStatsWidget>       StatsContent;       // Tab 1
TObjectPtr<UOccupationWidget>  OccupationContent;  // Tab 2
TObjectPtr<USpellListWidget>   SpellEditorContent; // Tab 3
TObjectPtr<UInnerWorldWidget>  InnerWorldContent;  // Tab 4

// 右上角全版切換頁（與 Tab 共用同一 content 區，覆蓋 Tab 內容）
// 按下引導/圖鑑/設定按鈕時，切換至對應全版頁；左上角返回鍵切回原 Tab
TObjectPtr<UGuideWidget>          GuidePage;
TObjectPtr<UCompendiumWidget>     CompendiumPage;
TObjectPtr<UPlayerSettingsWidget> SettingsPage;
```

### 6-C 資料更新

`ASkillCreatorHUD::DrawHUD()` 或 `Tick()` 改呼叫：
```cpp
PlayerPanel->RefreshStatsTab(Stats, Hp, MaxHp, Mp, MaxMp, Level, TierName, Equip);
// SpellListWidget 自行從 Character 取資料（現有機制不變）
```

---

## 七、Inventory + Equipment 並排

### 7-A 位置策略

目前 `UInventoryWidget` 和 `UEquipmentWidget` 各自由 HUD BeginPlay 在 Canvas 上定位。
`ToggleInventoryAndEquipment()` 實作：

```cpp
void ASkillCreatorHUD::ToggleInventoryAndEquipment()
{
    const bool bNewVisible = !InventoryPanel->IsVisible();
    if (bNewVisible) {
        InventoryPanel->SetVisibility(ESlateVisibility::Visible);
        EquipmentPanel->SetVisibility(ESlateVisibility::Visible);
        SetInputMode(UI + Game);  // 現有邏輯
    } else {
        InventoryPanel->SetVisibility(ESlateVisibility::Hidden);
        EquipmentPanel->SetVisibility(ESlateVisibility::Hidden);
        SetInputMode(Game only);
    }
}
```

兩者位置：物品欄維持現有位置，裝備欄靠右側排（現有位置已分開，不需調整）。

---

## 八、實作分段

### [x] Stage 1：鍵位重整 + R 鍵合併
- `ASkillCreatorPlayerController` 移除 V/B/T 綁定；G 改為 `OnOpenPlayerPanel`；R 改為 `OnOpenInventoryAndEquipment`
- `ASkillCreatorHUD` 加 `TogglePlayerPanel()` / `ToggleInventoryAndEquipment()`（暫時 PlayerPanel 為 null，不崩潰）
- Build 通過確認鍵位正確

### [x] Stage 2：UPlayerPanelWidget 框架
- 建立 `UPlayerPanelWidget`（4 Tab + 3 圓形按鈕 + 空白內容區）
- `ASkillCreatorHUD::BeginPlay()` 建立 `PlayerPanel` → G 鍵可開關這個空框架

### [x] Stage 3：整合現有子 Widget
- 把 `UStatsWidget`、`USpellListWidget` 從 HUD 成員移入 `UPlayerPanelWidget` 內部持有
- Tab 1（Stats）、Tab 3（SpellEditor）接上子 Widget
- Tab 2（Occupation）、Tab 4（InnerWorld）建立空白佔位 Widget
- 更新 `ASkillCreatorHUD::DrawHUD()` 資料餵送路徑

### [x] Stage 4：UPlayerSettingsWidget + UInputSettingsWidget 改造
- 建立 `UPlayerSettingsWidget`（含「操作細節」checkboxes + 嵌入 `UInputSettingsWidget`）
- 把舊 `USettingsWidget` 的 `OnHoldToPlaceChanged` / `OnPerfectRemoveChanged` 邏輯搬到此處
- `UInputSettingsWidget` 加入「點擊行 → 黃框等待 → 多鍵偵測 → 儲存」狀態機；`UKeyRebindProxy` 繞過 Dynamic Delegate 不支援 `AddLambda` 的限制
- 刪除 `USettingsWidget`；HUD 移除 SettingsPanel/StatsPanel/SpellListPanel/InputSettingsPanel

### [x] Stage 5：引導 / 圖鑑 佔位
- 建立 `UGuideWidget`、`UCompendiumWidget`（空白 Border + 「施工中」標籤）
- 右上角按鈕接上

---

## 九、涉及的主要檔案

| 動作 | 檔案 |
|------|------|
| **新建** | `UPlayerPanelWidget.h/.cpp` |
| **新建** | `UOccupationWidget.h/.cpp` |
| **新建** | `UInnerWorldWidget.h/.cpp` |
| **新建** | `UGuideWidget.h/.cpp` |
| **新建** | `UCompendiumWidget.h/.cpp` |
| **新建** | `UPlayerSettingsWidget.h/.cpp` |
| **修改** | `UInputSettingsWidget.h/.cpp`（多鍵 UI 改造） |
| **修改** | `ASkillCreatorHUD.h/.cpp`（成員/API 重整） |
| **修改** | `ASkillCreatorPlayerController.cpp`（鍵位） |
| **刪除** | `USettingsWidget.h/.cpp` |

---

## 十、設計決策（已確認）

| # | 問題 | 決策 |
|---|------|------|
| D-1 ✅ | Tab 4 名稱 | **內部空間** |
| D-2 ✅ | 引導/圖鑑的顯示方式 | **切換整個畫面**，左上角加返回鍵 + 標題（與舊版技能編輯頁面相同模式） |
| D-3 ✅ | 鍵位組合鍵定義 | **視為多個獨立 `FKey`**，存成 `TArray<FKey>`（最多 4 個），與現有 legacy BindKey 架構一致 |
| D-4 ✅ | 鍵位儲存位置 | **Config .ini**（沿用現有 `UInputSettingsWidget::ConfigFilePath()`），一台機器一份全局設定 |
