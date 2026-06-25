# W-10｜角色創建系統 實作計畫

> 來源：`origin text setting/about w - 10 playercreate.txt`（2026-06-21，使用者具體化版本，**取代**
> `plan-worldlore-integration.md` 裡 W-10a~f 的抽象描述，但 a~f 的分類仍可對照參考）。
> 種族資料來源：`Import word setting/ExportBlock-game-word/種族 *.md` 及其 14 份子文件
> （`常見種族體系與其血脈階級一覽 *.md` 是索引，逐一連到各「體系」文件）。
> `game-setting/選擇.md`（worldlore 計畫文件提到的步驟 4~10 設計來源）**目前專案內不存在**，
> 以使用者新寫的 txt 為唯一權威來源。
>
> **實作狀態：Phase A~E 全部完成（2026-06-21 Build 通過，2026-06-25 使用者實機 PIE 確認）。**
> 本文件保留設計規格與決策紀錄。使用者原話：「先把整體架構、步驟給做出來，細節後續再慢慢調整」——
> 所以下面刻意把「現在做」和「先空白/先簡化，以後補」分開標注，不要超做。

---

## 一、現狀

`UGameFlowWidget::BuildCharCreateScreen()`（`UGameFlowWidget.h/.cpp`）目前只是「輸入名字 + 確認創建」
一頁式畫面，對應 Godot `GameFlowUI.cs:257-303`。**這個畫面會被整個換掉**，改成下面的 11 步精靈流程。
`FCharacterSaveData`（`CharacterSaveData.h`）目前完全沒有種族/外貌/能力點/負面效果/靈魂萬象/體質/
心靈眾影響任何欄位——這些都是全新資料，不是「Godot 已有邏輯搬過來」，所以不適用 CLAUDE.md
「遷移 Godot 邏輯前必讀原始碼」那條規則（這是新內容，適用的是「新增內容時必看」那段慣例）。

---

## 二、整體架構決策

### 2.1 新 Widget：`UPlayerCreateWidget`（獨立類別，不是塞進 UGameFlowWidget）

理由：11 個步驟、每步驟內容差異極大（純文字輸入 / 圓球選單 / 點數分配器 / 確認卡），硬塞進
`UGameFlowWidget` 會讓那個類別過度肥大。`UGameFlowWidget::OnCharCreateNavClicked()` 改成建立並顯示
`UPlayerCreateWidget`（取代現在導向 `CharCreateScreen` 的行為），完成後（「確定創建」）呼叫
`UGameFlowWidget::CreateCharacter(...)` 寫回存檔，再 `RemoveFromParent()` 收掉精靈、切回角色選擇畫面。

🔴 **`NativeOnInitialized()` 建 WidgetTree，不是 `NativeConstruct()`**——本次 session
才剛修完 `UGameFlowWidget` 因為這個誤用造成的「永遠空畫面」bug（見 `docs/開發血汗錄.md` 第 1 案、
CLAUDE.md「新增 UI Widget」段）。新類別從第一天就要用對的寫法，不要重蹈覆轍。

### 2.2 步驟陣列：「容易增減/調整順序」的具體做法

使用者明確要求：「流程本身的數量、順序有可能（變動），請你實作出一個容易增減流程、調整順序的
架構模式」。具體設計：

```cpp
// UPlayerCreateWidget.h
struct FPlayerCreateStep
{
    FText Title;                          // 左上角流程標題
    TFunction<UWidget*()> Build;          // 建立這一步的內容（只呼叫一次，建出來後快取）
    TFunction<bool()>     CanAdvance;     // 「下一步」是否可按；nullptr = 永遠可以
    bool bShowQuickCreate = false;        // 這一步要不要顯示右上角「快速建立」
};

TArray<FPlayerCreateStep> Steps;   // 順序 = 陣列順序，BuildLayout() 裡 push_back 11 次
int32 CurrentStepIndex = 0;
TArray<UWidget*> StepWidgets;      // 跟 Steps 一一對應，Lazy build 後快取
```

`BuildLayout()` 裡用 11 個 `Steps.Add({...})` 依序註冊（對應下面第四節逐步規格），之後要加新步驟、
刪步驟、換順序，都只是調整這個陣列裡的呼叫順序/增減項目，**不用改外層導航邏輯**——外層的
「上一步/下一步/標題/快速建立」按鈕、`ShowStep(Index)` 顯示切換、`CanAdvance()` 驗證呼叫，全部
只認陣列索引，不認步驟本身是什麼。這跟 `UGameFlowWidget::ShowScreen()` 現有的「畫面陣列 +
SetVisibility 切換」模式是同一招，只是從固定具名欄位換成可變長度陣列。

外層 chrome（每步驟共用）：
- 左上角：`Steps[CurrentStepIndex].Title`
- 右下角按鈕：文字隨步驟變化（"下一步" / 最後一步 "確定創建"）
- 左下角「上一步」：`CurrentStepIndex == 0` 時 `SetVisibility(Collapsed)`
- 右上角「快速建立」：`Steps[CurrentStepIndex].bShowQuickCreate` 為真才顯示（步驟 1~10 開，
  步驟 11 關——已經沒有東西可以跳過了）

🔁 **資訊卡編輯跳轉**（對應第六節決策 #4）：額外一個 `int32 ReturnToStepIndex = -1` 狀態。
資訊卡（步驟 11）每個項目旁的「編輯」按鈕設 `ReturnToStepIndex = CurrentStepIndex（也就是 10)`
後 `ShowStep(目標步驟Index)`。右下角按鈕的 `OnClicked` 判斷：若 `ReturnToStepIndex >= 0`，
按下後改成 `ShowStep(ReturnToStepIndex); ReturnToStepIndex = -1;`（文字也要對應改成「確認」），
**不要**走一般的 `++CurrentStepIndex`，否則會把中間步驟全部重新跑一次。

### 2.3 「空白流程」的最小實作

PS 定義（使用者原文）：「該流程頁面空白，只包含左上角流程標題，以及右下角的下一步按鈕」。
對應到 `FPlayerCreateStep`：`Build = []{ return MakeBlankStep(); }`（一個只有 Border 背景、
什麼互動元件都沒有的 `UWidget*`），`CanAdvance = nullptr`。步驟 5/7/8/9/10 第一版全部用這個
（步驟 7 例外，見下方第四節，需要驗證邏輯但畫面仍是空的）。

---

## 三、新增資料結構

### 3.1 種族登記表 `FRaceRegistry`（新檔案，`Source/SkillCreatorCore/`）

放在 `SkillCreatorCore`，不是 `SkillCreatorRuntime`——跟 `FItemRegistry`/`FMaterialRegistry`
同一個理由：純資料、無 gameplay 依賴，UI（`UPlayerCreateWidget`）跟之後可能需要種族加成的
`AbilitySystem`（worldlore W-10a 提到「MP 親和度、血脈能力」）都能直接依賴，不會有
「低層 plugin 反過來依賴高層」的問題（這正是 2026-06-20 把 `TotemLibrary` 從 `SkillCreatorUI`
移到 `AbilitySystem` 的同一個教訓，見 CLAUDE.md「新增刻印/圖騰資料」段）。

```cpp
// RaceRegistry.h
UENUM() enum class ERaceTier : uint8 { Yellow, Profound, Earth, Heaven }; // 黃/玄/地/天

USTRUCT() struct FRaceDefinition
{
    FName     Id;            // "Human", "DwarfKin"...
    FName     SystemId;      // 對應到哪個「體系」（人類體系/精怪體系/萬獸體系...）
    FText     DisplayName;
    ERaceTier Tier = ERaceTier::Yellow;
    FText     Description;   // ≤100 字
};

USTRUCT() struct FRaceSystemDefinition
{
    FName SystemId;
    FText DisplayName;       // 圓球按鈕上顯示的體系名稱
};

class FRaceRegistry
{
public:
    static void Initialize();   // Register() 呼叫集中於此，跟 FItemRegistry::Initialize() 同模式
    static const TArray<FRaceSystemDefinition>& AllSystems();
    static TArray<const FRaceDefinition*> RacesInSystem(FName SystemId);
    static int32 CostForTier(ERaceTier Tier); // 黃0/玄10/地50/天100，對應使用者規格第 3 點
};
```

### 3.1.1 資料範圍（2026-06-21 更新：使用者選擇「填完所有體系」）

讀完 `Import word setting/ExportBlock-game-word/` 底下全部 16 份「體系」索引文件後的結論：

**先講三個不適合直接照單全收的體系**（建議排除或特殊處理，不要照原計畫無腦塞進創建清單）：

| 體系 | 問題 | 建議 |
|------|------|------|
| 神界種族（皆為神族） | 內容全是「天神/佛/菩薩/魔神/星辰/異神/神種/神仙」——這些是**飛升後的神格狀態**，不是玩家一開局能選的種族，文件本身也寫「任何達到神之領域並來到神界的物種皆保有原先種族特性」，意思是底層種族不變，只是飛升後多一個神格頭銜 | **不放進創建清單**。圓球可以列出名稱（忠實對應「所有檔案中提到的體系」），但點進去清單是空的並標註「飛升後狀態，無法在創建時選擇」 |
| 冥界種族 | 骷髏/殭屍/木乃伊/幽靈/鬼物多半是**死亡/詛咒後的轉化狀態**，不是「生來就是」的種族；且分級用的是「1 階/2 階」，不是黃/玄/地/天 | 可以放，但 Tier 對應方式見下方備註；本質上也比較像「死後轉職」而非「創建時選擇」，先放進清單，但等實際做這步驟時可能要重新考慮是否該出現在這裡 |
| 靈界種族 | 天使/惡魔/精靈/英靈本身是「魂的根源分類」，文件描述其需要「契合度高的容器」才能受肉於凡間，比較像「附身/契約對象」而非「玩家一出生就是」的種族 | 同上，先放（標 Tier::Heaven，因為描述的力量等級都極高），但跟冥界種族一樣是待確認的灰色地帶 |

**Tier 對應備註**：靈界/冥界/神話/聖靈/邪靈幾個體系，原文沒有統一用黃/玄/地/天標記（神話體系
是文件標題直接寫「全天階」；冥界用「1 階/2 階」；靈界完全沒標）。下表裡沒有明確標記的，
一律按描述的力量強度，由實作者對應到最接近的 Tier；冥界「2 階」對應 `ERaceTier::Profound`（玄階）。

**以下是全部 16 體系、所有「有清楚名稱」的種族清單**（區分「有現成簡述可直接用」vs「只有連結
到子文件、沒有內文敘述」——後者實作時要點開對應的子文件才能寫出敘述，這次規劃沒有逐一追進去，
因為很多子文件本身又連到更深的子文件，追到底會變成把整個世界觀文件庫搬過來，超出「角色創建
架構規劃」的範圍）：

**1. 人、類人體系**（黃階，全部有現成敘述）：人族、矮人族、侏儒族、哈頓族、長耳族、紅獵族
（取自 `人、類人體系...674c680a....md`；半妖族/森精族/狼人族/半血族/人鬼族/人魚族另有獨立
敘述但牽涉劇情設定較深，先跳過，子文件連結都還在原文件裡）

**2. 精怪體系**：黃階（有敘述：花妖族、蕈菇族、地精族、魔靈族、仙靈族、器靈族、建樓族、神兵族、
魅魔族、光族、七色族；連結待補：樹妖族、妖精族、森精族）、玄階（有敘述：炎靈族、冰魄族、雷鳴族、
厚土族；連結待補：德魯伊族）、地階（有敘述：寧芙族，僅讀到第一條，子文件還有更多未讀）

**3. 寄生體系**：黃階（天手族、妖髮族、幼臉族，皆有敘述）、玄階（魔眼族，有敘述）

**4. 天行體系**：分「具魔」「不具魔」兩支，各自再分黃/玄/地階。
具魔：黃階（詭鴉族、白鴿族、水鴨族、只雞族、魔蝠族、貓頭鷹族，有敘述；連結待補：翼人族）、
玄階（孔雀族、五彩鸚鵡族、天馬族，有敘述）、地階（金鵬族、獅鷲族、三足金烏族，有敘述）。
不具魔：黃階（鷹族、赤翼蛇族、疾燕族、鵜鶘族、海鷗族、碧鵝族，有敘述；連結待補：天行妖族）、
玄階（仙鶴族、翼鵰族，有敘述）

**5. 肢節體系**：黃階（鐮刀螂族、獵蛛族、魔蜂族、血蚊族、魔瞳蠅族、土蚯族、黑蜈蚣族、魔蠍族、
巨翅蟑族，有敘述；連結待補：甲蟲族、鬥蟻族、肢節妖族）、玄階（幻蝶族、夢蠶族——只有名稱，
沒有敘述也沒有連結，文件本身就只寫了名字）、地階（黑金魔蟬族，同樣只有名稱）

**6. 水行體系**：黃階（海魚族、深海魚族、蟹族、蝦族、軟爪族、魔棘族、刺胞族，有敘述；連結待補：
人魚族、水行妖族）、玄階（巨鯨族、九爪族、蜃族，有敘述）、地階（魚龍族、巴哈姆特族，有敘述）

**7. 半神體系**：玄階（千翼族、千手族、無常族，有敘述）、地階（天狐族、天狗族，有敘述；連結待補：
邪根族）

**8. 萬獸體系**：黃階（犬族、貓族、半人馬族、狂牛族、神象族、戰羊族、蜥蜴族、彩鹿族，有敘述；
連結待補：鬼族、哥布林族、鬼兔族、獸人族、歐克族、萬獸妖族）、玄階（恐龍族、奇美拉族、天蛇族，
有敘述）、地階（貔貅族、月蟾族，有敘述）

**9. 次元物種**：玄階（影族、幻想蟲族、元素靈族，有敘述；機械族連結待補）、地階（幻形族、深淵族，
有敘述）

**10. 神話體系**（全天階）：神龜族、羽蛇神族、四神族、四凶族、比蒙族、利維坦族，有敘述；連結待補：
翼神龍族、瑞神龍族、鳳族、巨人族、泰坦族、麒麟族、天球族、夢幻族、克蘇魯族

**11. 邪靈體系**：黃階（詭異族，連結待補）、玄階（夜叉族、巫妖族，有敘述；血族/邪根族連結待補）、
地階（暗夜妖族、海魔族、暴食族，有敘述）

**12. 聖靈體系**：黃階（耀精族、幻靈惡族，有敘述）、玄階（獨角族、歌訟族，有敘述）、地階
（超負義族，有敘述）

**13. 靈界種族**（見上方灰色地帶說明，暫標 Tier::Heaven）：天使、惡魔、精靈、英靈——全部只有
總覽段落敘述，連結待補

**14. 冥界種族**（見上方灰色地帶說明）：「1 階」（骷髏、殭屍、木乃伊、幽靈、鬼物、克爾柏洛斯，
有敘述）、「2 階」→ 對應 `ERaceTier::Profound`（亡靈、鬼怪、死神，有敘述）

**15. 神界種族**：**不放進創建清單**（見上方說明）

**16. 人造物種**：內容極少（活化複合生體/雲奴/屍奴，幾乎都只有名稱），先列系統名稱，種族清單
留空，等子文件內容補完再填

**統計**：有現成敘述可直接寫進 `Register()` 的種族粗估約 **75 種**（不含連結待補、不含排除的神界
種族）；連結待補（需要實作時額外開子文件才能寫敘述）約 **25 種**。這已經遠超 worldlore 計畫原本
「初期 5–8 種」的範圍，但使用者已確認「反正只是選項而已，目前本質上沒有區別」，所以 Phase C 直接
照這份清單把「有現成敘述」的種族一次性 `Register()` 完，連結待補的種族等之後個別需要時再補。
（2026-06-21 實作時逐一核對寫進 `RaceRegistry.cpp`，實際數字是 **119 種**，比這裡粗估的 75 高
不少——粗估時漏算了肢節/水行/天行等體系裡其實也有不少「有敘述」的種族。）

### 3.2 `FCharacterSaveData` 新增欄位

```cpp
// 種族（W-10 step 3）
UPROPERTY() FName RaceId;

// 基礎能力點（W-10 step 6，7 項，初始皆 0）
UPROPERTY() int32 BasePoint_Physique  = 0;  // 體魄
UPROPERTY() int32 BasePoint_Strength  = 0;  // 肌力
UPROPERTY() int32 BasePoint_Endurance = 0;  // 耐力
UPROPERTY() int32 BasePoint_Agility   = 0;  // 敏捷
UPROPERTY() int32 BasePoint_Intellect = 0;  // 智慧
UPROPERTY() int32 BasePoint_Charisma  = 0;  // 魅力
UPROPERTY() int32 BasePoint_Luck      = 0;  // 幸運
```

⚠️ **決策待確認**：worldlore 計畫 W-10d 原本設計是「創建能力點」（一次性花費額度，創建角色時
分配掉就沒了）+「天賦能力點」（倍率係數，初始 50 點，決定每點基礎能力點換算成多少實際數值
加成）兩種角色創建階段專用的點數軌；但使用者這次的新 txt 規格**只提到「創建能力點」**，
步驟 6 直接拿創建能力點 1:1 分配到 7 項基礎能力點，完全沒提天賦能力點。

（這兩種點數軌都跟 W-8「貨幣系統」——靈石/魔晶/銅錢/泰克通用幣，世界裡可流通交易的真正貨幣
——完全無關，純粹是創建角色那幾分鐘內部用的一次性配點機制，創建完就不存在了。上一版這裡誤用
「雙貨幣設計」這個詞描述創建/天賦點數的關係，用詞不準，容易誤會成跟 W-8 有關聯，已更正。）

這是簡化版本，還是漏寫？**先照新 txt（單一創建能力點）做**，因為使用者明確說「以後再慢慢調整」，
天賦能力點之後要補可以再加一個 `TalentPoint_*` 平行欄位，不影響這次架構。

步驟 4（外貌）/5（喜好）/7（負面效果)/8（靈魂萬象)/9（體質)/10（心靈眾影響）**這次都不加
資料欄位**——4 是空白流程的時候不需要、5/8/9/10 明確要求空白流程、7 雖不空白但只是「檢查
創建能力點 ≥ 0」不需要新欄位。等對應步驟真的要做內容了再加，現在加只是預先設計用不到的抽象，
違反 CLAUDE.md「不要設計超出任務需要的抽象」。

### 3.3 創建期間的暫存狀態（不存檔，只存在 `UPlayerCreateWidget` 內）

```cpp
FString CreationName;
int32   CreationPointsTotal = 0;   // 步驟2 隨機抽出，50~100
int32   CreationPointsSpent = 0;   // 即時算剩餘 = Total - Spent
FName   SelectedRaceId;
TMap<FName,int32> BaseStatAllocations; // 7 項，步驟 6 用
```

走完步驟 11「確定創建」才一次性寫進 `FCharacterSaveData` 並呼叫既有的
`FFlowSaveSystem::CreateNewCharacter`/`SaveCharacter`（簽名可能要從「只給 Name」擴充成
「給整份 FCharacterSaveData 預填值」，需要的話加一個 overload，不要破壞現有呼叫點）。

---

## 四、11 步驟逐一規格

| # | 標題 | 內容 | 驗證 | 本次範圍 |
|---|------|------|------|----------|
| 1 | 輸入角色名字 | `UEditableText`（不是 `UEditableTextBox`，要支援多語輸入；UMG 的 `UEditableText` 底層走 Slate `SEditableText`，中英文數字標點都吃，不用額外處理） | 無（沿用現有「空白則預設『旅者』」慣例，不卡關） | 做 |
| 2 | 設定創建能力點總數 | `FMath::RandRange(50,100)` 顯示於中央；「設置最高能力點」按鈕直接設 100 | 無 | 做 |
| 3 | 設定種族 | 體系圓球（`FRaceRegistry::AllSystems()`，用 `UButton` + 圓形 `UBorder` Brush 排成散落佈局——可先簡化成 `UWrapBox` 一排，散落座標效果之後再做）→ 點擊後 `UScrollBox` 清單（複用既有 `UFlowCardWidget` 那種「整塊可點擊卡片」模式）→ 每張卡顯示名稱/階級/敘述/所需點數，點選後 `CreationPointsSpent += CostForTier(Tier)` | 必須選滿一個種族才能下一步（`CanAdvance = [this]{ return !SelectedRaceId.IsNone(); }`） | 做（資料只填人類體系黃階 6 種，見 3.1） |
| 4 | 設定外貌 | — | 無 | **空白流程**（捏臉系統規模太大，獨立排期，不是這次架構任務範圍） |
| 5 | 設定喜好 | — | 無 | **空白流程**（使用者明確指示） |
| 6 | 設定基礎能力點 | 仿「人生重開模擬器」：頂部即時顯示剩餘創建能力點，7 列（體魄/肌力/耐力/敏捷/智慧/魅力/幸運）各一排「− 數字框（可直接輸入) +」 | 無顯式擋（但 + 按鈕在剩餘點數為 0 時應該直接 disable，使用者沒明說但這是這類介面的基本行為） | 做 |
| 7 | 設定負面效果 | — | 點「下一步」時檢查 `CreationPointsTotal - CreationPointsSpent < 0` → 跳出提示視窗擋下 | **空白流程 + 驗證邏輯**（驗證要做，畫面不用） |
| 8 | 抽取靈魂萬象 | — | 無 | **空白流程**（內部仍標 8-1~4 四個子項名稱供未來對照，不建資料欄位） |
| 9 | 抽取體質 | — | 無 | **空白流程**（同上，9-1~4） |
| 10 | 心靈眾影響 | — | 無 | **空白流程**（同上，10-1~4） |
| 11 | 最終角色資訊確認 | 「角色資訊卡」列出步驟 1~10 所有已決定內容（空白流程的項目就不列，或列「待設定」）；每個項目旁有「編輯」按鈕，點擊後跳回對應步驟（見 2.2 的 `ReturnToStepIndex` 機制） | 「確定創建」寫存檔 + 收尾；「重新建立」回到步驟 1（`CurrentStepIndex=0`，清空暫存狀態） | 做 |

---

## 五、「快速建立」邏輯

```cpp
void UPlayerCreateWidget::OnQuickCreateClicked()
{
    if (CreationName.IsEmpty()) CreationName = TEXT("旅者");
    if (CreationPointsTotal == 0) CreationPointsTotal = FMath::RandRange(50, 100);

    // 隨機挑一個目前已有資料、且點數買得起的種族（先排除超出剩餘點數的選項）
    SelectedRaceId = PickAffordableRandomRace(CreationPointsTotal);
    CreationPointsSpent = FRaceRegistry::CostForTier(/*該種族 Tier*/);

    // 剩餘點數隨機灑進 7 項基礎能力點（不用很聰明，亂數分配掉就好）
    RandomlyAllocateRemainingPoints();

    CurrentStepIndex = 10; // 直接跳第 11 步（index 從 0 起算）
    ShowStep(CurrentStepIndex);
}
```

對應使用者規格：「點擊該按鈕後，要能自動、隨機地走完流程1~10，直接跳至流程11」——空白流程
（4/5/7/8/9/10）天然不需要隨機化，只有 1/2/3/6 真的有東西要隨機填。

---

## 六、決策結果（2026-06-21 使用者已確認）

1. **創建能力點 vs 天賦能力點**——確認：沿用單一創建能力點軌，不做天賦能力點。
2. **種族資料填多少**——確認：填完所有體系。已展開為 3.1.1 的完整 16 體系清單
   （約 75 種有現成敘述，25 種連結待補，神界種族整個排除）。
3. **步驟 3 的「圓球散落佈局」**——確認：先用 `UWrapBox` 規則排列代替真正散落視覺。
4. **步驟 11 資訊卡編輯方式**——確認：資訊卡上每個項目可以點擊跳回對應步驟，玩家在那一步
   調整完，點擊該步驟畫面右下角的按鈕（這一步驟此時文字應顯示「確認」而不是「下一步」）
   直接跳回資訊卡，**不會**重新走一遍中間其它步驟。實作上：`CurrentStepIndex` 額外記一個
   `int32 ReturnToStepIndex = -1`；資訊卡的「編輯」按鈕設
   `ReturnToStepIndex = 10`（資訊卡是 index 10）後跳到目標步驟；該步驟右下角按鈕的文字/行為
   改成「若 `ReturnToStepIndex >= 0`，按下後直接 `ShowStep(ReturnToStepIndex)` 並清空
   `ReturnToStepIndex`，不要 `++CurrentStepIndex` 往下一步走」。

---

## 七、檔案清單（已建立，Phase A~E ✅）

**新增：**
- `Source/SkillCreatorCore/Public/RaceRegistry.h` + `Private/RaceRegistry.cpp`
- `Source/SkillCreatorRuntime/Public/UPlayerCreateWidget.h` + `Private/UPlayerCreateWidget.cpp`
- （可選）`Source/SkillCreatorRuntime/Public/UStatAllocatorWidget.h`——步驟 6 的「−數字+」一排
  獨立小元件，仿 `UParamSpinWidget` 的封裝模式（同樣用 `NativeOnInitialized()`）

**修改：**
- `Source/SkillCreatorRuntime/Public/CharacterSaveData.h`（加 3.2 的欄位）
- `Source/SkillCreatorRuntime/Public/UGameFlowWidget.h` / `.cpp`
  （`OnCharCreateNavClicked()` 改建立 `UPlayerCreateWidget` 而不是切到 `CharCreateScreen`；
  `BuildCharCreateScreen()` 和 `CharCreateScreen` 欄位可以整個移除）
- `Source/SkillCreatorRuntime/Public/FlowSaveSystem.h` / `.cpp`
  （`CreateNewCharacter` 可能需要 overload 接受預填好的 `FCharacterSaveData`）

---

## 八、實作階段拆分（建議，供下次動工排序）

> 2026-06-21：Phase A~E 全部完成並 Build 通過（兩個 target 皆 0 錯誤 0 警告）。
> 2026-06-25：使用者實機測試通過，11 步流程確認可用。
> 詳見 `實作進度.md`「UE5 最新完成」表格對應列。

1. [x] **Phase A（架構骨架）**：`UPlayerCreateWidget` + `FPlayerCreateStep` 陣列機制 + 11 個
   空白佔位步驟（先全部用「空白流程」UI，只有標題+上一步+下一步能動）+ 串接
   `UGameFlowWidget::OnCharCreateNavClicked()`。先確認整個 11 步流程能走通、「快速建立」
   能跳到 11，再進 Phase B。
2. [x] **Phase B（步驟 1/2 實裝）**：名字輸入 + 創建能力點抽取/設最高。
3. [x] **Phase C（`FRaceRegistry` + 步驟 3 實裝）**：實際完成時把 §3.1.1 列出的 16 體系全部
   有現成敘述的種族（共 119 種，比原估計的「約 75 種」更多——重新逐一核對時把肢節/水行等
   體系也數進去了）全部 `Register()` 進去（神界種族除外）+ 圓球/清單 UI + 點數扣除。「連結
   待補」的種族仍未補（先用名稱+「敘述待補」佔位的構想最後沒有實作，這些種族目前完全不在
   清單裡，等之後個別需要時再讀子文件補上）。
4. [x] **Phase D（步驟 6 實裝）**：`UStatAllocatorWidget` + 7 項基礎能力點分配介面。
5. [x] **Phase E（步驟 7 驗證 + 步驟 11 資訊卡 + 存檔串接）**：負面效果守衛邏輯、最終確認卡
   （含每項「編輯」按鈕 + `ReturnToStepIndex` 跳轉邏輯，見 2.2）、`CreateNewCharacter` overload、
   收尾流程。
6. Phase A~E 完成＝W-10 架構任務的「整體架構、步驟」達標。外貌/喜好/靈魂萬象/體質/
   心靈眾影響的**實際內容**留給對應的未來 session（worldlore 計畫 W-10b/c/f 仍可作為那幾次的
   設計參照起點）。
