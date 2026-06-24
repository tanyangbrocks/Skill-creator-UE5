# 物品分類擴充 + 加工/合成系統 實作計畫

> 撰寫日期：2026-06-23
> 來源規格：[`origin text setting/base item system.txt`](../origin text setting/base item system.txt)
> 狀態：**✅ 已完成（2026-06-23，全 7 步，Build 0 錯誤 0 警告；詳見 `實作進度.md`「UE5 最新完成」表）**
> 性質：本文件大部分內容是 **Godot 沒有的全新系統**（不是遷移）。已逐一核對 `C:\skill-creator\Scripts\World\Items\` 確認 Godot 原版只有「工具/裝備/方塊/材質碎片」四類、3 個裝備欄（Weapon/Armor/Accessory）、單一 ToolTier+MiningSpeedMult，沒有加工、寶箱、合成臺、互動系統。本計畫是在現有 UE5 物品架構基礎上的**新設計**，不適用 CLAUDE.md「先讀 Godot 源碼」規則（無源可讀），但沿用既有架構慣例。

---

## 一、現有架構掃描結果

| 系統 | 檔案 | 現狀 |
|------|------|------|
| 物品 ID | `Source/SkillCreatorCore/Public/ItemId.h` | `EItemId` enum，目前 23 個值（方塊5/工具3/裝備3/礦石4/碎片9） |
| 物品資料 | `Source/SkillCreatorCore/Public/ItemData.h` | `FItemData`：`bIsPlaceable`/`PlaceAs`、`bIsTool`/`ToolTier`/`MiningSpeedMult`、`MaxStack`、`EquipSlot`/`AtkMult`/`DefFlat`/`MpBonus` |
| 物品登錄表 | `Source/SkillCreatorCore/Public/ItemRegistry.h` + `.cpp` | `FItemRegistry::Get(Id)`，靜態初始化 |
| 裝備欄 | `Source/SkillCreatorCore/Public/EquipmentSlotType.h` | 只有 3 欄：`Weapon`/`Armor`/`Accessory` |
| 背包 | `Source/SkillCreatorRuntime/Public/UInventoryComponent.h` | 30 格（10 熱鍵+20 主欄），`TryAdd`/`Consume`/`SwapSlots`，**單一**元件，無「多個 inventory 來源」概念 |
| 背包 UI | `Source/SkillCreatorRuntime/Public/UInventoryWidget.h` + `.cpp` | 綁死「畫一個 30 格」，`Refresh(const UInventoryComponent*)` 已是參數化，可重用畫單個 inventory，但**不支援雙欄並排拖曳** |
| 放置 | `Source/SkillCreatorRuntime/Private/ASkillCreatorCharacter.cpp:1045 OnPlace()` | 右鍵固定＝放置 tile（`Data.bIsPlaceable && PlaceAs != Air` 才有效），**沒有 fallback 到「與物件互動」** |
| 採掘 | 同檔 `OnMine()` | 左鍵固定＝採礦，讀 `InventoryComp->GetActiveToolTier()/GetActiveMiningSpeedMult()`，**全域單一倍率，不分材質類別** |
| 放置物登記 | `Plugins/VoxelWorld/.../PlacedObjectRegistry.h/.cpp` + `PlacedUnit.h` | 已有「多格 tile 構造物」追蹤（採掘/拉伸用），但 `FTileCell` 是純 POD（見 CLAUDE.md），**無法附帶 inventory 資料** |
| 高亮 | `AVoxelWorldActor.cpp ShowHighlight()` | 採礦用，`DrawDebugBox(Cyan, 0.05s)`，每幀依準心 raycast 結果重畫 |
| 互動介面 | （不存在） | 專案目前沒有 `IInteractable` 之類的介面 |
| 攻擊/使用道具 | （不存在） | 左鍵只接 `OnMine`，沒有「手持武器→攻擊」「手持道具→使用」分支 |

---

## 二、三個必須先拍板的架構衝突點

### 衝突 1：右鍵現在＝放置，規格要右鍵＝互動

`OnPlace()`（`ASkillCreatorCharacter.cpp:1045`）目前無條件對著準心 raycast 命中的 tile 放置物品。規格要求「右鍵＝與面前事物交互」（開寶箱、用合成臺）。

**建議方案**：在 `OnPlace()` 最前面插入「互動優先」判定——對準心方向做一次「找最近 Interactable」的 trace（半徑用 `WorldScale::MiningRangeTiles` 或更短的互動距離），若命中且在範圍內，呼叫 `Interact()` 並 `return`，否則才繼續走現有的放置流程。互動目標（寶箱/合成臺）是獨立 `AActor`（見§六、七），不是 tile，所以這個 trace 走的是 UE5 物理/Overlap 查詢，跟 tile raycast（`TW->Raycast`）是兩條平行的判定路徑，不衝突。

### 衝突 2：裝備欄 3 槽 → 規格要 5 槽，且武器不算裝備欄

`EquipmentSlotType.h` 現在是 `None/Weapon/Armor/Accessory`。規格寫「裝備：頭盔、鎧甲、褲子、鞋子、飾品五個欄位」——**武器不在這 5 欄裡**，武器是規格裡獨立的「★武器」分類，跟「★工具」一樣是熱鍵欄持握物，攻擊力直接讀 `力量(Strength) × AtkMult` 即時計算，不走裝備欄加成。

**✅ 已確認（2026-06-23）**：使用者指出 `EEquipmentSlotType` 之後高機率會持續加新欄位，**不要用固定 `UENUM`**。改用跟 `FRaceRegistry`/`FManaTypeRegistry` 同一套「登錄表」模式（這是本專案既有慣例，不是新發明的架構）：

```cpp
// EquipmentSlot.h（取代 EquipmentSlotType.h）
USTRUCT(BlueprintType)
struct FEquipmentSlotDef
{
    GENERATED_BODY()
    UPROPERTY() FName Id;          // "Helmet"/"Armor"/"Pants"/"Boots"/"Accessory"
    UPROPERTY() FText DisplayName;
    UPROPERTY() int32 SortOrder = 0;   // 裝備欄 UI 排列順序
};

struct SKILLCREATORCORE_API FEquipmentSlotRegistry
{
    static const TArray<FEquipmentSlotDef>& GetAll();
    static const FEquipmentSlotDef* Find(FName Id);
};
```

- `FItemData.EquipSlot` 型別從 `EEquipmentSlotType` 改成 `FName`（`NAME_None` = 不可裝備）
- `UEquipmentComponent` 內部從「固定欄位 `WeaponId/ArmorId/AccessoryId`」改成 `TMap<FName, EItemId> EquippedBySlot`（5 欄變多欄完全不影響這個元件的程式碼，只是 Map 多一個 key）
- **這個改法的實際好處**：之後新增裝備欄只需要在 `FEquipmentSlotRegistry::Init()`（`.cpp` 內的純邏輯）多加一行 `Register()`，屬於 CLAUDE.md「只改 .cpp 純邏輯」的安全範疇，**不需要關閉 Editor + 完整 Rebuild**（固定 `UENUM` 每次加值都要走完整 Rebuild）；UI（裝備欄面板）改成依 `FEquipmentSlotRegistry::GetAll()` 動態產生欄位，不用每次新增欄位都改 UI 排版程式碼
- 傷害計算從「裝備欄查表」改成「讀目前熱鍵欄選中物品，若 `bIsWeapon` 則 `Damage = Strength * AtkMult`」——這跟 `ASkillCreatorCharacter::TakePhysicalDamage` 已有的力量公式管線（見 [實作進度.md](../實作進度.md) B 系列）是同一條路徑，只是傷害輸入源從「固定值」換成「武器公式」

⚠️ 這是 **breaking change**：現有 `EquipBasicSword`（`EquipSlot=Weapon`）必須改成 `bIsWeapon=true` 且不再佔用裝備欄。**「無正式玩家存檔」的意思澄清**：不是說存檔系統本身有問題，純粹是專案目前還在開發階段，沒有真正的玩家在用這套裝備系統長期遊玩、累積出需要相容的存檔資料——現在改 slot 定義，沒有「破壞既有玩家進度」的風險，跟存檔系統的可靠性無關。

### 衝突 3：左鍵現在＝採礦，規格要依「按壓時長」+「手持物類別」雙軸分派

**✅ 已修正（2026-06-23）**：使用者澄清實際規則跟原草案不同，正確邏輯是：

> 「長按左鍵」都是挖掘；「短按左鍵」都是用手中物體攻擊。只有當手中持有道具時，無論長按、短按左鍵都是使用道具。

也就是分派的第一軸是「按壓時長」（長按→挖掘／短按→攻擊），**`bIsConsumable` 是覆寫規則**，蓋掉時長判斷，兩種按法都變成使用道具。

**建議方案**：UE5 Enhanced Input 原生支援同一個實體鍵位（`LeftMouseButton`）綁兩個不同 Trigger 的 Input Action，比手動計時更乾淨：

```cpp
UInputAction* IA_PrimaryTap  = MakeBool(TEXT("PrimaryTap"));   // UInputTriggerTap
UInputAction* IA_PrimaryHold = MakeBool(TEXT("PrimaryHold"));  // UInputTriggerHold（沿用現有 IA_Mine 的 Triggered 連續觸發語意）
// 兩個 IA 都 MapKey(LeftMouseButton)，各自加對應 TriggerType
```

- `OnPrimaryTap()`（短按放開觸發一次）：若手持物 `bIsConsumable` → `UseConsumable()`；否則 → 攻擊判定（沿用既有 Contact/melee 範圍掃描邏輯，傷害＝力量公式，攻擊判定細節留給實作階段，見開放問題 Q3）
- `OnPrimaryHold()`（持續觸發，沿用現有 `OnMine()` 邏輯）：若手持物 `bIsConsumable` → `UseConsumable()`（跟短按同一個效果，長按不會重複觸發多次使用，只觸發一次，沿用 `bRightMouseWasPressed` 同款 rising-edge 防重複機制）；否則 → 原 `OnMine()` 採礦邏輯（工具只影響採礦速度）
- 兩個 handler 共用同一個 `UseConsumable()` 私有函式，避免重複程式碼

> 對應既有規劃：`docs/plan-player-actions.md` 第 60/119 行已經把「左鍵輕攻擊 / 採掘」列為同一顆鍵位的已知重疊（標記「無正式攻擊框架」），本計畫的 `OnPrimaryTap()` 攻擊分支就是接到那份文件 §E 攻擊框架（`AttackType`/`AttackHitbox`/`AttackHitAction`）尚未拍板前的暫定 stub，見開放問題 Q3。

---

## 三、`EItemId` 新增清單

延續現有命名風格（`Block*`/`Tool*`/`Equip*`/`Ore*`/`Fragment*`），新增以下分類前綴：

```
// ── 武器（新分類）──────────────────────────────
WeaponWoodSword,

// ── 工具（補上既有缺的木系全套）──────────────────
ToolWoodShovel,   // 木鏟
ToolWoodAxe,      // 木斧（現有 ToolBasicAxe 語義重疊，見下方§九開放問題 Q1）
ToolWoodPick,     // 木鎬（現有 ToolBasicPick 語義重疊，同上）

// ── 裝備（補滿 5 欄；現有 3 件全部要重新分類，見下表）──
EquipLeatherHelmet,
EquipLeatherPants,
EquipLeatherBoots,
// EquipLeatherArmor / EquipAmulet 沿用現有值，EquipSlot 改掉即可

// ── 素材（規格已列的木系半成品）───────────────────
MaterialPlank,     // 木材（原木加工出來的中間產物）
MaterialStick,     // 木棍

// ── 可放置物（新分類，會 spawn Actor，不是 tile）───
PlaceableWoodChest,        // 木寶箱
PlaceableWoodWorkbench,    // 木工作臺

// ── 建議新增（規格沒明寫，但合理補滿木系迴圈，可調整/可不做）──
ToolStonePick,     // 石鎬（沿用「升級鏈」慣例，呼應現有 ToolIronPick 已經是下一階）
WeaponStoneSword,  // 石劍
ConsumableBerry,   // 野莓（規格提到「道具」分類但沒給範例，補一個最小可用的消耗品占位）
```

> **✅ 已確認（2026-06-23，修正第二輪）**：原木／木材拆成兩個獨立 ID，**兩者都可放置，且維持玩家可自選形狀**（不是固定尺寸——上一輪草案曾提議「1 遊戲單位立方體」固定尺寸，使用者已撤回此想法）：
> - `BlockWood`（沿用現有 ID）＝原木，`bIsPlaceable=true`
> - `MaterialPlank`（新 ID）＝木材，`bIsPlaceable=true`（原草案誤判木材「無放置語義」，已修正）
>
> 兩者都跟 `BlockDirt`/`BlockStone` 等既有方塊物品走同一條路徑：`PlaceAs=EMaterialType::Wood`，吃玩家在形狀選單（`UShapeMenuWidget`）選的 `Shape`/`Radius`，**不需要任何新欄位**。

---

## 四、`FItemData` 欄位擴充

```cpp
// 新增分類旗標（沿用現有 bool flag 風格，物品可同時具備多個）
bool bIsWeapon      = false;  // ★武器
bool bIsMaterial    = false;  // ★材質（地圖採掘掉落物，標籤用）
bool bIsComponent   = false;  // ★素材（可用於加工，標籤用）
bool bIsConsumable  = false;  // ★道具（左鍵使用）

// 工具類別化（解決衝突 3 之外的「木鏟只加速挖土」需求）
EToolCategory ToolCategory = EToolCategory::None;  // 新 enum：None/Shovel/Axe/Pickaxe

// 可放置物的「放成 Actor」分支（寶箱/工作臺用，跟既有 PlaceAs(tile) 二選一）
TSubclassOf<APlacedFixtureActor> PlaceAsActor = nullptr;  // 非 null 時 OnPlace() 改走 spawn Actor 路徑
```

### 可放置物再細分：可塑形 / 不可塑形（2026-06-23 新增）

**✅ 使用者要求**：「可放置」要再分兩類——**可塑形**（吃玩家選的形狀/半徑，像挖地一樣可以用 Cube/Sphere/Cylinder/Flat 任意形狀放）跟**不可塑形**（永遠是固定的單一物件，玩家不能改它的形狀）。目前清單裡只有「木寶箱」「木工作臺」是不可塑形，其餘所有可放置物（`BlockDirt/BlockStone/BlockWood/BlockSand/BlockAsh/MaterialPlank/Ore*` 等）都是可塑形。

這個分類**剛好對應現有的兩條放置路徑，不需要新增任何欄位**——兩者是自然互斥的（一個物品不會同時兩個欄位都填）：

| | 可塑形 | 不可塑形 |
|---|---|---|
| 判斷依據 | `PlaceAs != EMaterialType::Air` | `PlaceAsActor != nullptr` |
| 放置結果 | tile 材質，吃 `HUD->ActiveShape`/`PlaceRadius` | `SpawnActor<APlacedFixtureActor>`，固定單一物件 |
| 範例 | 原木、木材、泥土、石頭、礦石 | 木寶箱、木工作臺 |

可以加兩個 inline helper 方便其他系統判斷，不重複儲存資料：
```cpp
bool IsMoldablePlaceable()    const { return bIsPlaceable && PlaceAs != EMaterialType::Air; }
bool IsNonMoldablePlaceable() const { return bIsPlaceable && PlaceAsActor != nullptr; }
```

> 順帶一提（不阻塞本計畫，記下來即可）：`UShapeMenuWidget` 目前是全域形狀選單，不管手持什麼都能開、能選。等寶箱/工作臺這類不可塑形物品做出來後，比較好的體驗是手持「不可塑形」物品時自動隱藏/停用形狀選單（反正選了也不會生效）——這是 UI 細節優化，不影響資料層設計，可以留到實作階段再做。

`MaterialType.h` 同步新增 `EMaterialCategory`（`None/Soil/Wood/Stone/Ore`）標記在既有材質登錄表（`MaterialRegistry.cpp`）上，`ToolCategory` 與 `EMaterialCategory` 比對才給 `MiningSpeedMult` 加成，否則只用 `ToolTier` 判斷能不能挖（沿用現有邏輯，只是加一層「類別匹配才加速，類別不匹配仍可挖但沒加速」）。

對應規格：
> 木鏟（能加速挖掘土）、木斧（能加速挖掘木製品）、木鎬（能加速挖掘石頭、礦物）

---

## 五、互動系統（右鍵 Interact + 綠色高亮）

新增純虛擬介面（C++ 系統間，依 CLAUDE.md 慣例不用 UINTERFACE，但因為要 `Cast<>` 到 `AActor` 上的元件，用 UE5 標準 `UINTERFACE`+`IInteractable` 雙生宏更合適——這是要給 Blueprint/AActor 用的對外接口）：

```cpp
UINTERFACE(BlueprintType)
class UInteractable : public UInterface { GENERATED_BODY() };

class IInteractable
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent) void Interact(ASkillCreatorCharacter* Player);
    UFUNCTION(BlueprintNativeEvent) FText GetInteractPrompt() const;  // 之後可選：互動提示文字
};
```

- `AVoxelWorldActor` 或新的 `AInteractionTraceComponent`（建議獨立成 `ASkillCreatorCharacter` 的子元件，跟現有 mining raycast 平行）每幀對準心方向做 Overlap/LineTrace，找最近實作 `IInteractable` 的 Actor，距離 ≤ 互動距離（建議新常數 `WorldScale::InteractRangeTiles`，规格沒給具體數字，可先沿用 `MiningRangeTiles` 或設更短）
- 命中時：呼叫高亮（綠色，沿用 `ShowHighlight()` 同款 `DrawDebugBox` 機制，顏色換成 `FColor::Green`，跟現有採礦高亮（青色）視覺上能同時存在但不會搶同一個 tile——一個高亮的是 Actor 的碰撞框，一個是 tile 格子）
- `OnPlace()` 開頭呼叫這個 trace，命中就 `Interact()` 並 return（見衝突 1）

---

## 六、寶箱系統

> 木寶箱／木工作臺是目前清單裡僅有的「不可塑形」可放置物（見 §四），所以這兩節都走 `PlaceAsActor` 路徑，不吃形狀選單。

### 資料設計
- 新增 `APlacedFixtureActor`（基底類，寶箱/工作臺共用）：持有 `UInventoryComponent`（重用現有元件，不重新發明）+ `UStaticMeshComponent`（暫用引擎 Cube，跟 `AEnemy::MeshComp` 同款做法）+ 實作 `IInteractable`
- `AChestActor : APlacedFixtureActor`：`UInventoryComponent::TotalSize` 設成跟玩家背包一樣（規格「相當於玩家物品欄總數」），`Interact()` 觸發開啟雙欄 UI（見下方）

### 放置流程
- `EItemId::PlaceableWoodChest` 的 `FItemData.PlaceAsActor = AChestActor::StaticClass()`
- `OnPlace()` 偵測到 `PlaceAsActor != nullptr` 時走新分支：算出放置座標（沿用現有 `PlaceCenter` 計算），`SpawnActor<APlacedFixtureActor>`，**同時**在該 tile 位置寫一個輕量 `EMaterialType::Fixture`（新材質，純粹當「這格被占用、不能再放/不能採」的標記，不影響 Greedy Meshing 顯示——Actor 自己有 mesh）
- 存讀檔：仿照 `PlacedObjectRegistry` 既有的 `placed-registry.json` 模式，新增 `world-fixtures.json`（`[{pos, classId, inventorySlots}]`），世界載入時讀回並 `SpawnActor`

### UI 設計
- `UInventoryWidget` 目前綁死單一 inventory 的版面常數（`TotalSlots=30` 等都是 `static constexpr`），建議**不要改這個類別**，而是新增 `UChestWidget`：水平排列「玩家背包 `UInventoryWidget` 實例」+「寶箱背包 `UInventoryWidget` 實例」（兩個獨立 widget 並排），各自的 `OnSlotSwapRequested` 都接到 HUD 層，HUD 新增 `OnCrossInventoryTransferRequested(SrcWidgetId, SrcSlot, DstWidgetId, DstSlot)` 統一處理跨欄拖曳（`UInventoryComponent` 之間直接搬 `FItemStack`，不是同一元件內的 `SwapSlots`）
- `Interact()` 時：`ASkillCreatorHUD` 同時開玩家物品欄面板（現有 Z 鍵邏輯）+ 寶箱面板，兩者並排（對應規格「玩家打開寶箱，玩家自己的物品欄也會自動打開」）

---

## 七、合成臺（工作臺）系統

### 資料設計
- `AWorkbenchActor : APlacedFixtureActor`：不需要自己的 `UInventoryComponent`（合成臺本身不存物品，只是「附近有沒有工作臺」的判定錨點）
- `Interact()` 行為：規格其實沒說「對工作臺按右鍵」要做什麼特別的事——工作臺的作用是「玩家站在附近，加工選單自動偵測到有工作臺」，不是互動開菜單（那是規格§3 預告的「操作臺」，本計畫不做）。所以 `AWorkbenchActor::Interact()` 可以留空或顯示一個簡短提示，不是本系統的核心路徑。

### 範圍判定
新增 `UCraftingStationSubsystem`（`UWorldSubsystem`，比照現有 `UCombatStateSubsystem` 模式）：
- 維護目前世界所有 `AWorkbenchActor` 的位置清單（Spawn/Destroy 時註冊/取消註冊，不需要每幀掃描全世界）
- `bool HasNearbyWorkbench(FVector PlayerLoc, float RadiusGameUnits = 5.f)`：`RadiusGameUnits * WorldScale::GrainCurrent * WorldScale::TileSizeCm` 換算成 cm 距離比較（規格「5 個遊戲單位」，對照 `WorldScale.h` 既有換算慣例）

---

## 八、加工系統

### 資料設計
```cpp
USTRUCT() struct FRecipeInput { EItemId ItemId; int32 Count; };

USTRUCT() struct FRecipeEntry
{
    EItemId Output;
    int32   OutputCount;
    TArray<FRecipeInput> Inputs;
    bool    bRequiresWorkbench = false;   // 規格「需工作臺」的配方標記 true
};

// 對應 Godot 模式：靜態登錄表，仿 FItemRegistry::Init
struct FRecipeRegistry
{
    static const TArray<FRecipeEntry>& GetAll();
};
```

初始配方（直接照規格抄錄，`原木`＝重用 `BlockWood`，`木材`＝新 `MaterialPlank`，`木棍`＝新 `MaterialStick`）：

| 輸入 | 輸出 | 需工作臺 |
|------|------|---------|
| 1× BlockWood（原木） | 4× MaterialPlank（木材） | 否 |
| 1× MaterialPlank | 4× MaterialStick（木棍） | 否 |
| 4× MaterialPlank | 1× PlaceableWoodWorkbench | 否 |
| 1× MaterialStick + 2× MaterialPlank | 1× WeaponWoodSword | 是 |
| 2× MaterialStick + 3× MaterialPlank | 1× ToolWoodAxe | 是 |
| 2× MaterialStick + 3× MaterialPlank | 1× ToolWoodPick | 是 |
| 2× MaterialStick + 1× MaterialPlank | 1× ToolWoodShovel | 是 |
| 8× MaterialPlank | 1× PlaceableWoodChest | 是 |

> 規格原文「1根木棍、2個木材，可加工為1把木劍」重複寫了兩次（第 55、56 行），已去重。

### 可加工判定演算法
```
每幀（或物品欄/寶箱內容變動時觸發，不需要每幀重算）：
1. 蒐集「可用素材池」= 玩家 Inventory.Slots + （若有寶箱在 5 遊戲單位內）寶箱 Inventory.Slots，依 ItemId 加總數量
2. 對 FRecipeRegistry::GetAll() 逐筆檢查：
   a. bRequiresWorkbench 且 !HasNearbyWorkbench() → 跳過
   b. 素材池每個 Input 數量 >= 需求 → 此配方「可加工」
3. 可加工清單變動時才觸發 UI 重建（不要每幀 ClearChildren，參考本次稽核發現的 RebuildList 同類問題）
```

### 消耗順序
規格：「當玩家和附近寶箱持有相同且需被消耗的素材時，優先消耗玩家身上的素材」——逐個 `FRecipeInput` 先嘗試 `PlayerInv.Consume()`，數量不足的差額再從寶箱 `Consume()` 補。

### UI 設計
新增 `UCraftingPanelWidget`：
- 5×5 網格（規格「由左向右排最多五個，由上而下排最多五排」），每格是縮小版 `UBlockCardWidget` 風格的方形圖卡（重用既有 `UParamTinyLabelWidget` 之類的小元件風格，不需要新框架）
- 超過 25 筆：改成一個「選單按鈕」+ 點擊彈出可捲動側欄（`UScrollBox`），同樣的圖卡排列
- Hover 顯示 tooltip（名稱＋所需素材清單）——直接重用 `UInventoryWidget` 已有的 `ShowFloatTooltip`/`HideFloatTooltip` 模式（`NativeTick` 偵測 hover，同一招）
- 點擊圖卡＝立即加工：呼叫消耗邏輯＋`PlayerInv.TryAdd(Output, OutputCount)`
- 面板常駐顯示（規格沒提觸發鍵，比對「玩家左側中央立刻出現」的描述，這是**永遠可見**的 HUD 一部分，不是按鍵開關的面板——比照生存條 HUD 的呈現方式，常駐在 `UPlayerHUDWidget` 左側）

---

## 九、開放問題（需要使用者確認）

1. **`ToolWoodAxe`/`ToolWoodPick` 跟現有 `ToolBasicAxe`/`ToolBasicPick` 是否是同一把？** 現有兩件已經有 `ToolTier`/`MiningSpeedMult` 數值且大概率已被某處引用（存檔/掉落表）。建議：重新命名現有兩個 ID 的 `DisplayName`／調整數值對齊規格的「木斧」「木鎬」定位，**不新增重複 ID**，但 `EItemId.h` 裡的識別碼名稱（`ToolBasicAxe`）保留不改（避免存檔相容性問題），純粹是 `FItemData` 內容對齊規格。本計畫第三節的清單已經反映這個建議（只新增 `ToolWoodShovel`，斧/鎬沿用現有 ID）。
2. **✅ 已確認（第二輪修正）**：`BlockWood`＝原木、新 `MaterialPlank`＝木材，**兩者都可放置，且維持玩家可自選形狀**（撤回「1 遊戲單位立方體」固定尺寸的提案），歸類為「可塑形」。已反映在第三、四節，順帶新增「可放置物再細分：可塑形/不可塑形」的分類（對應現有 `PlaceAs`/`PlaceAsActor` 兩條路徑，不需要新欄位）。
3. **✅ 已確認**：武器攻擊判定屬於 `docs/plan-player-actions.md` §E 攻擊框架的範疇（更複雜，依賴該文件未拍板的「路線A/路線B」決策），**這次可以不做**——`OnPrimaryTap()` 的攻擊分支先留 stub（例如直接呼叫現有 `ExecuteContactHit()` 佔位，或完全留空待之後接），不阻塞本計畫其餘部分。等 `plan-player-actions.md` 的攻擊框架決策確定後，再回頭把 `bIsWeapon`/`AtkMult` 接進那套系統。
4. **「道具」子分類完全沒有規格範例**（規格說「有很多子分類」但沒舉例），本計畫只建立 `bIsConsumable` 旗標+左鍵 dispatch 骨架，等規格補充後再設計效果系統。
5. **互動距離數字** 規格沒給，需要使用者拍板或先用一個暫定值（建議 `MiningRangeTiles` 的一半，避免穿牆互動）。
6. **寶箱/工作臺存檔格式**（`world-fixtures.json`）是新檔案，需要確認是否要跟現有 `placed-registry.json` 合併還是分開——本計畫傾向分開（語義不同：一個是「tile 構造物」，一個是「帶 inventory 的 Actor」），但兩者最終都要在 `FFlowSaveSystem::SaveAll()` 加呼叫。

---

## 十、建議實作順序

1. `EItemId`/`ItemData`/`MaterialType` 資料層擴充（§三、四）—— 低風險，先做
2. 裝備欄 3→5 改造（§衝突2）—— 影響面小但是 breaking change，盡早做完避免後面疊加
3. `IInteractable` 介面 + 右鍵 dispatch 改造（§衝突1、五）
4. `APlacedFixtureActor`/`AChestActor`/`AWorkbenchActor` + 放置流程改造（§六、七）
5. 寶箱雙欄 UI（§六 UI 設計）
6. `FRecipeRegistry` + 加工判定 + `UCraftingPanelWidget`（§八）
7. 左鍵 `OnPrimaryTap`/`OnPrimaryHold` Tap/Hold dispatch（§衝突3）—— 攻擊分支留 stub，不等 `plan-player-actions.md` 攻擊框架決策也能先做（開放問題 3 已確認可延後）

每個階段完成後依 CLAUDE.md 規則更新 [實作進度.md](../實作進度.md)，`.h` 異動（新 enum/新 UCLASS）一律關閉 Editor + 完整 Rebuild。
