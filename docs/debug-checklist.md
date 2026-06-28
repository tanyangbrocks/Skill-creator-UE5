# SkillCreator UE5 — 靜態 Debug 清單

> **定位**：這份文件收錄「預檢腳本找不到、但也不需要實機測試即可發現」的 bug 類型。
> 預檢腳本（`preflight-check.ps1`）做的是結構性完整性檢查；這份文件補足它的盲點——邏輯正確性、UE5 語意陷阱、跨檔案一致性。

---

## 類別 A：用 Grep 可以快速掃到的（優先做）

### A-1 UObject* 未用 UPROPERTY 保護（GC 靜默回收）

**風險**：GC 在任何一幀把未受保護的 UObject 指標回收，下一幀 dereference 直接 crash，且日誌不會指向根因。

**掃法**：
```powershell
# 找 .h 裡看起來像 UObject 成員欄位但前面沒有 UPROPERTY 的行
grep -rn "U[A-Z]\w\+\*\s\+\w\+" Source/ Plugins/ --include="*.h" | grep -v "UPROPERTY\|//\|TSubclassOf\|TWeakObjectPtr"
```

**判斷標準**：  
- 確認欄位前一行是否有 `UPROPERTY()`  
- `TWeakObjectPtr` 本身 GC 安全（不計）  
- 純 local 變數 / 函式參數不計，只看 class 成員

---

### A-2 虛擬函式 Super:: 遺漏

**風險**：`BeginPlay`/`Tick`/`NativeOnInitialized` override 忘呼叫 `Super::` 會跳過引擎初始化步驟——元件未 tick、輸入未綁定、UMG 樹未建立，且不是 compile error。

**掃法**：
```powershell
# 找實作了這些函式但緊接的下一行不含 Super:: 的情況
grep -rn "::\(BeginPlay\|Tick\|NativeOnInitialized\|EndPlay\)()" Source/ Plugins/ --include="*.cpp" -A 2 | grep -v "Super::"
```

**判斷標準**：  
- 不是所有 override 都需要 `Super::`（純葉節點 / 刻意不呼叫有文件說明的），逐個確認

---

### A-3 TakeSnapshot 漏抓 runtime 可變欄位

**風險**：`TakeSnapshot()` 若漏掉 runtime 可變欄位，Anchor/Rollback 積木執行後該欄位不回到快照前狀態（靜默失效）。

**2026-06-25 掃描結果**：
- `ABeastCharacter::BeastKind`/`Hostility` — constructor-set 不可變，不需要快照，正確
- `ANPCCharacter::Disposition` — **FIXED**: 可被攻擊改為 Hostile，TakeSnapshot/RestoreFromSnapshot 原本漏存；已加入 `FEntitySnapshot::bHasDisposition + DispositionByte`（uint8 opaque cast）修復

**掃法（未來新增欄位時用）**：
```powershell
grep -n "TakeSnapshot\|RestoreFromSnapshot" Source/SkillCreatorRuntime/Private/ANPCCharacter.cpp
# 確認 TakeSnapshot 的賦值列表，對照 .h 裡所有可變（非 const/不在 ctor 設定）欄位
```

**判斷標準**：  
- 有 runtime setter 的欄位 → 必須進快照  
- 只在 constructor/BeginPlay 設定、之後不改的欄位 → 不需要快照

---

### A-4 TMap 查詢誤用 `[]` 建立空條目

**風險**：`TMap::operator[]` 找不到 key 時會**插入預設值**（不 crash、不回 nullptr），導致資料結構無聲膨脹、後續邏輯讀到空殼條目。應用 `Find()` 查詢、`FindOrAdd()` 才是刻意插入。

**掃法**：
```powershell
# 檢查各個 TMap 變數的 [] 存取
grep -rn "Chunks\[.\|Registry\[.\|Slots\[.\|ItemMap\[." Source/ Plugins/ --include="*.cpp"
```

**判斷標準**：  
- 只有「確定 key 存在」或「刻意插入預設」的情況才能用 `[]`  
- 「查詢但不確定是否存在」一律改 `Find()`

---

## 類別 B：需要讀 Godot 原始碼比對（需要更多時間）

### B-1 Godot Y-up → UE5 Y-down 符號反轉（座標公式）

**2026-06-25 掃描結果**：
- `SurfaceWaterPool::QueryOverride()` — 碗形公式 ✓（BowlDepth/FloorY 符號正確）
- `SurfaceWaterPool::Prepare()` — **FIXED**: 水面 Y 公式用了 `(1-WaterFill)` 而非 `WaterFill`；Godot: `naturalH - MaxDepth*WaterFill`，UE5 正確: `CenterH + MaxDepth*WaterFill`，修前水池只有 30% 滿而非 70%
- `FMapGenerator3D::GetHeightAt()`/`WorldScale::TileToWorld()` — 地形上已正確使用，沿用既有修正

**確認方式（未來新增高度公式時）**：對照 Godot `SurfaceWaterPool.cs`/`MapGenerator3D.cs`，逐行比對有號數計算，Godot `-=X` 在 UE5 改 `+=X`。

---

### B-2 Delegate 有廣播但無訂閱者（或反過來）

**2026-06-25 掃描結果**：
- `OnExplosionComplete` ✓ — 廣播 AVoxelWorldActor.cpp:147，訂閱 UDroppedItemManager.cpp:16 AddUObject
- `OnVoxelDestructionComplete` ✓ — 廣播 AVoxelWorldActor.cpp:292，訂閱 UDroppedItemManager.cpp:17 AddUObject
- `OnHpChanged` — 無訂閱者，但 HUD 從 `ASkillCreatorHUD::DrawHUD()` 每幀 polling，不需要 delegate → 正常
- `OnAttackHit` / `OnParrySuccess` — 無訂閱者；核心功能 inline 完成（不靠 delegate），此 delegate 供未來 on-hit 咒語效果掛鉤，目前尚未實作訂閱端 → 不是現時 bug
- `OnServerReady` — 廣播存在，無訂閱者；ANPCCharacter::TriggerDialogue 同步呼叫 IsReady() 不依賴此 delegate → 不是現時 bug

**確認方式**：對每個 multicast delegate，分別 grep `Broadcast(` 和 `AddUObject(` / `AddDynamic(`，確認訂閱者存在且 lifetime 合理。

---

### B-3 Godot 預設值核對（數值未移植 / 移植錯誤）

**2026-06-28 核對結果**：

| 系統 | Godot 值 | UE5 值 | 結論 |
|------|---------|--------|------|
| MP 係數 Instant/Declare/Sustained | 0.8 / 1.0 / 1.5（`AbilityPointCalculator.cs:11`） | `FAbilityPointCalculator::GetMpMultiplier` 0.8/1.0/1.5 | ✅ 一致 |
| 重力加速度 | 30 tile/s²（`PlayerController.cs:143`） | `GravityScaleMult=TileSizeCm/30`→有效重力=980/30=32.67 tile/s² | ⚠️ ~9% 偏差；但 JumpZVelocityCm=TileSizeCm×14 使跳高 = 精確 3 tile，設計意圖明確 |
| GrowthSlow speedPenalty | 0.15/stack, 5 stack 上限（`ElementalStatusEffect.cs:60-62`） | `AbnormalStatusEffects.h` 系統不同（GAS vs Elemental Aura） | N/A：UE5 走 GAS GE Blueprint，非 C++ 硬碼 |
| FrozenEffect 效果 | `DamageTakenBonus=0.20`（`ElementalStatusEffect.cs:117`） | `FFrozenStatus::GetDefensePenalty()=1.0f` | ⚠️ 機制不同（增傷 20% vs 防禦歸零）；Godot 原始碼標 `⚠️ 待平衡`，視為設計改動 |
| FrozenEffect duration | 1.0s（`ElementalStatusEffect.cs:118`） | `FFrozenStatus` duration=2.0s | ⚠️ 倍增；同上，待平衡 |
| Frostbite→Frozen 機制 | 無（Godot 無凍傷疊層系統，走 Water+Ice 直接反應） | `USpecialStatusComponent::ProcessEffects()`：FFrostbiteStatus 5 層疊滿觸發 | UE5 新增系統，非移植 |

**總結**：無緊急數值 bug。兩項 FrozenEffect 差異是 UE5 設計改動（Godot 原作者自標 `⚠️ 待平衡`）；重力略偏高但跳高正確。

---

## 類別 C：UE5 特有語意陷阱（需要了解 UE5 內部機制）

### C-1 Build.cs 模組依賴遺漏（Unity Build 掩蓋）

**風險**：Unity Build 把同模組所有 `.cpp` 合併成一個 TU，A.cpp 靠別的 cpp 帶入的 include 隱式獲得 B.h，但 Build.cs 沒有宣告對 B 的依賴。換台機器或增量編譯順序改變時才炸 LNK 錯誤。

**確認方式**：對新增的跨模組型別（如 `UNPCBrainSubsystem`、`FCaGpuSimulator`），確認使用它的模組的 `Build.cs` 有對應 `PublicDependencyModuleNames` 或 `PrivateDependencyModuleNames`。

---

### C-3 AddDynamic 傳非字面量（Runtime Assertion 炸 Delegate.h:474）

**風險**：`AddDynamic` 是 C++ 巨集，用 `#FuncName` 在預處理期字串化第二個參數。若傳入陣列元素（`Callbacks[i]`）或變數，字串化結果是 `"Callbacks[i]"` 而非 `"UMyClass::MyMethod"`，不含 `::`，Delegate.h:474 assertion `Result[2] != '0'` 爆炸，只在 Runtime 才崩潰，沒有 compile error。

**正確用法**：`AddDynamic(this, &UMyClass::OnSomeEvent)` — 第二個參數必須是編譯期可見的成員函式指標字面量。

**錯誤用法（不可做）**：
```cpp
void (UMyClass::* Callbacks[])() = { &UMyClass::OnA, &UMyClass::OnB };
Btn->OnClicked.AddDynamic(this, Callbacks[i]);  // 炸 Assertion
```

**正確替換**：用 `switch(i)` 讓每個 case 直接傳字面量，或使用非動態 delegate（`AddWeakLambda`/`AddUObject`，但需確認 delegate 類型支援）。

**preflight-check.ps1 13aa 已自動偵測此模式。**

### C-2 CDO 狀態污染（PIE vs Standalone 差異）

**風險**：PIE 重複 Play 時，UObject 的 Class Default Object 可能保留上一次 Play 的狀態（靜態成員、單次初始化的 TArray 等），Standalone 每次都是全新進程，行為不同。

**高風險模式**：函式體內的 `static` 變數 + UObject constructor 裡的初始化邏輯。

---

### C-4 BindRaw 在 UObject 上 → GC 後懸空指標

**風險**：`Delegate.BindRaw(this, &UMyClass::Method)` 對 UObject 是危險的——`BindRaw` 不持有弱參考，GC 回收 `this` 後 delegate 執行時直接 crash，且無法偵測。

**正確替換**：
- 非同步 HTTP / 廣播 delegate → `BindWeakLambda(this, [this](...) { ... })`（UE5 會在呼叫前檢查 weak 有效性）
- 一般 delegate → `BindUObject(this, &UMyClass::Method)`（持有 strong reference，但不 GC-safe；適用於確定 owner lifetime 的情境）

**掃法**：
```powershell
grep -rn "BindRaw\s*(\s*this" Source/ Plugins/ --include="*.cpp"
# 任何 match 都應改為 BindUObject 或 BindWeakLambda
```

**preflight-check.ps1 13ab 已自動偵測。**

---

### C-5 Async/Thread lambda 直接捕獲 UObject `this`

**風險**：`Async(EAsyncExecution::Thread, [this]() { ... })` 中，lambda 可能在 `this` 被 GC 回收後才執行，造成 null dereference 或 use-after-free。

**正確模式**：
```cpp
TWeakObjectPtr<UMyClass> WeakThis(this);
Async(EAsyncExecution::Thread, [WeakThis]()
{
    if (!WeakThis.IsValid()) return;
    WeakThis->DoWork();
});
```

或使用 `BindWeakLambda(this, [this](...) { ... })`（delegate context 內的 async callback）。

**本專案現況**：`UNPCBrainSubsystem` HTTP callback 已正確使用 `BindWeakLambda`。VoxelWorld 背景 chunk 生成的 lambda 捕獲的是 `TSharedPtr` 資料（非 UObject 指標），也安全。

**未來新增 Async 呼叫時確認**：lambda capture list 中的 UObject 指標一律改成 `TWeakObjectPtr`。

---

### C-6 前進 TArray 迴圈內呼叫 RemoveAt → 跳元素

**風險**：
```cpp
for (int32 i = 0; i < Array.Num(); ++i)
{
    if (ShouldRemove(Array[i]))
        Array.RemoveAt(i);  // 移除後 i 不減，下一個元素被跳過
}
```
靜默跳元素，邏輯不炸但行為錯誤。

**正確模式**：
```cpp
for (int32 i = Array.Num() - 1; i >= 0; --i)
    if (ShouldRemove(Array[i])) Array.RemoveAt(i);
// 或用 RemoveAll：Array.RemoveAll([](const T& E) { return ShouldRemove(E); });
```

**本專案現況**：所有移除迴圈（USpecialStatusComponent、UDroppedItemManager 等）已確認使用反向迴圈。

---

### C-7 GetWorld() 在純 UObject 建構子裡呼叫 → 回傳 null

**風險**：CDO 建構期（Class Default Object 初始化，發生在引擎啟動）呼叫 `GetWorld()` 回傳 null；`UActorComponent` / `AActor` 的 constructor 也是 CDO context，不可在此呼叫 `GetWorld()`。正確位置是 `BeginPlay()` 或 `Initialize()`。

**掃法**：
```powershell
# 找在 ::ClassName() 建構子函式體內直接呼叫 GetWorld() 的情況
grep -rn "GetWorld()" Source/ Plugins/ --include="*.cpp" -B5 | grep -B5 "GetWorld()" | grep "::[A-Z]\w*()"
```

---

### C-8 UWidget* 快取在 WidgetTree 重建後失效

**風險**：若程式碼先快取 `UTextBlock* MyLabel` 指標，之後呼叫 `WidgetTree->RebuildWidget()` 或 Widget 被 Removed 再 Added，舊指標指向已釋放的物件。

**本專案模式**：所有 Widget 用 `UPROPERTY(meta=(BindWidget))` 或在 `NativeOnInitialized()` 一次性初始化後不重建，目前無此風險。

**未來警戒情境**：如果呼叫 `ClearChildren()` 後再重建 WidgetTree，要同步清空所有快取 UWidget 指標。

---

---

## 類別 D：GAS Loose Tag 特有陷阱（2026-06-28 新增）

### D-1 Loose Tag 引用計數失衡（stackable 狀態）

**風險**：`UAbilitySystemComponent::AddLooseGameplayTag()` 用引用計數追蹤 tag。若對同一個 StatusId 的每一層 stack 都 Add，而 Remove 只在最後一層消失時呼叫一次，計數就是 N-1≠0，tag 永遠殘留。

**正確模式**：
- **Add**：只在 0→1 transition（`GetStackCount(Id) == 0`）時 Add
- **Remove**：只在最後一層消失（`HasStatus(Id)` 為 false）時 Remove

**掃法**：
```powershell
# 確認 ApplyStatus 的 AddLooseGameplayTag 前面有 GetStackCount == 0 守衛
grep -n "AddLooseGameplayTag\|GetStackCount" Source/SkillCreatorRuntime/Private/USpecialStatusComponent.cpp
```

### D-2 Loose Tag 殭屍殘留（自然到期 / 批量清除路徑）

**風險**：狀態效果有多條移除路徑，每一條都要同步清除 ASC loose tag：
1. `RemoveStatus()`（玩家 API）
2. `ProcessEffects()`（每幀到期）
3. `ClearAll()`（死亡/場景切換）
4. `ClearCategory()`（特定類別強制清除）

漏掉任何一條都會讓 tag 在效果結束後繼續殘留，導致 `CanApply("Frozen")` 永遠回傳 true。

**確認方式**：確認以上四個函式都有 `RemoveLooseGameplayTag` 呼叫（可用 `-ShowDetails` 的 13z 項目確認）。

### D-3 即時效果（RemainingDuration≤0）不應加 tag

**風險**：即時效果（如 `InstantDeath`）在 `OnApply()` 觸發後不進 `ActiveEffects`，也不會觸發 `RemoveStatus()`。若在這之前就 Add tag，tag 永遠無法被 Remove。

**正確模式**：`AddLooseGameplayTag` 必須在 `if (Effect->RemainingDuration <= 0.f) return;` 判斷之後。

---

## 類別 E：多執行緒與生命週期（需要讀實作細節）

### E-1 GPU async readback 時序（CaGpuSimulator）

**風險**：`CaGpuSimulator::AsyncCells`（普通 `TArray`，非 atomic）由 render thread 填充，game thread 在 `TryCollectAsync()` 裡讀取。兩者之間只靠 `AsyncReadbackInFlight` atomic pointer 同步。若 game thread 在 render thread 填完之前就讀，資料不完整（partial read）。

**確認方式**：
```cpp
// CaGpuSimulator.cpp 裡 TryCollectAsync()
// 確認在讀 AsyncCells 之前有 FlushRenderingCommands() 或等效 fence
// 預期序列：AsyncReadbackInFlight->IsReady() → FlushRenderingCommands() → read AsyncCells
```
`AsyncReadbackInFlight->IsReady()` 只確認 GPU 完成，不保證 render thread 已把資料寫入 `AsyncCells`。需確認中間有 flush 或 CPU fence。

---

### E-2 背景 chunk 生成 lambda 的閉包生命週期（**已修 2026-06-28**）

**原始風險**：`EnsureChunksAround()` 捕獲 `TQueue* Queue = &ReadyQueue`（原始指標），`~FMapGenerator3D() = default` 不等待 in-flight task，若物件先析構 → lambda 的 `Queue->Enqueue()` dangling pointer。

**修法（2026-06-28）**：
- `MapGenerator3D.h`：`ReadyQueue` 改為 `TSharedPtr<TQueue<FPendingChunk, EQueueMode::Mpsc>>`
- Constructor：`ReadyQueue = MakeShared<TQueue<...>>()`
- `EnsureChunksAround()`：`TSharedPtr<TQueue<...>> Queue = ReadyQueue;`（按值捕獲 → lambda 持有 shared ref，queue lifetime ≥ lambda）
- `ResetGenerationState()`：`ReadyQueue = MakeShared<TQueue<...>>()`（拋棄舊 queue，舊世界 in-flight task 寫入孤立 queue 不影響新世界）
- `ApplyPendingChunks()`：`ReadyQueue->Dequeue(Pending)`

---

## 類別 G：UI 佈局與移動語意陷阱（2026-06-28 新增）

### G-1 Widget NativeConstruct 呼叫 BuildLayout()（首次顯示必定空白）

**風險**：`NativeConstruct()` 在 `AddToViewport()` → `TakeWidget_Private()` → `RebuildWidget()` 完成快取後才執行；此時 `WidgetTree->RootWidget` 已被快取為 null（`UserWidget.cpp:1203` 回退成空 SSpacer）。結果：首次顯示永久空白，且因 `MyWidget` 快取不重建，無法自動修復。

**正確模式**：`BuildLayout()` 呼叫必須在 `NativeOnInitialized()`（在 `CreateWidget()` 當下觸發，早於任何 `AddToViewport()`）。

**掃法（preflight 13ai 已自動偵測）**：
```powershell
grep -rn "NativeConstruct" Source/ Plugins/ --include="*.cpp" -A 5 | grep "BuildLayout"
```

---

### G-2 HUD Widget 預設 Visible 但設計圖無此元素

**風險**：`ASkillCreatorHUD::CreatePanel<T>()` 預設將 widget 設為 `Collapsed`。若緊接著呼叫 `SetVisibility(ESlateVisibility::Visible)`，該 widget 會常駐顯示。若設計圖沒有這個元素，結果就是畫面多出不應存在的 UI 區塊（如 `UCraftingHintCardWidget` 的「0 格」問題）。

**掃法（preflight 13aj 已自動偵測）**：對照設計圖確認每一個 `SetVisibility(Visible)` 調用的 widget 是否在設計圖中有對應元素。

---

### G-3 地形 FractalOctaves > 4 → Greedy Mesh 棋盤格格紋

**風險**：UE5 Greedy Mesh 把相鄰等高 tile 合併成大四邊形渲染。Fractal Brownian Motion ≥5 個 octave 在每 3~4 tile 尺度製造微凹凸，阻止 Greedy 合併，產生明顯棋盤格格紋。Godot 每 tile 單獨渲染，無此問題，故 Godot 端 7 octaves 移植到 UE5 必須改為 ≤4。

**掃法（preflight 13ak 已自動偵測）**：
```powershell
grep -n "SetFractalOctaves" Plugins/VoxelWorld/Source/VoxelWorld/Private/MapGenerator3D.cpp
```

---

### G-4 固定鏡頭模式 W/S 方向偏移（GetControlRotation().Yaw 而非春臂 Yaw）

**風險**：Isometric / SideScroll2D 相機模式下 `SpringArm->bUsePawnControlRotation = false`，春臂鎖定在固定世界旋轉（45°/90° Yaw）。但若 `Move()` 仍使用 `GetControlRotation().Yaw`，方向取決於滑鼠上次的位置，與視覺鏡頭朝向不同，W/S 移動感覺「奇怪」。

**正確模式**：`Move()` 需根據 `SpringArm->bUsePawnControlRotation` 選擇 Yaw 來源：
```cpp
const float YawDeg = SpringArm->bUsePawnControlRotation
    ? GetControlRotation().Yaw
    : SpringArm->GetComponentRotation().Yaw;
```

**掃法（preflight 13al 已自動偵測）**：確認 `Move()` 函式體有 `bUsePawnControlRotation` 條件分支。

---

### G-5 K 鍵飛行模式僅從 IsFalling() 進入（地面/跳躍同幀靜默失敗）

**風險**：`ToggleFlight()` 若僅以 `else if (GetCharacterMovement()->IsFalling())` 為進入飛行條件：
- 在地面直接按 K → 條件失敗 → 靜默忽略
- 跳躍按 Space 的同一幀按 K → `IsFalling()` 尚未更新（MOVE_Falling 在下一幀物理更新後才生效）→ 同樣靜默忽略

**正確模式**：改為無條件 `else`，允許從任意狀態進入飛行（由 `OnGuardBreakOrFly` 的更上層條件做守衛，如鎖定目標→破防優先）。

**掃法（preflight 13am 已自動偵測）**：確認 `ToggleFlight()` 無 `else if (.*IsFalling)` 限制。

---

### G-6 HUD 生存條順序（設計圖 vs 程式碼順序不一致）

**風險**：設計圖規定順序（左→右）為 **溫度 / 飢餓 / 口渴 / 氧氣 / 體力**。程式碼若使用固定位置 `StartX + i * GapX`（i=0 對應飢餓）則溫度顯示在最右側，與設計圖相反。

**確認方式**：對照 `UPlayerHUDWidget::BuildSurvivalBars()` 中溫度（Temperature）的 X 座標是否最小（最左）。

---

### G-7 MP 弧段方向（預設 CW，設計圖要求 CCW）

**風險**：`DrawArcSeg()` 中若 A0 隨 i 遞增（`A0 = start + i * (Arc + Gap)`），弧段順時針展開，與設計圖「逆時針消耗」方向相反。

**正確模式**：A0 隨 i 遞減（`A0 = start - i * (Arc + Gap)`），DrawArcSeg 傳入 A1 < A0（負 span），讓函式支援 CCW 弧。

**確認方式**：`UHpMpCircleWidget::NativePaint()` 中 MP 弧段迴圈，確認 A0 遞減而非遞增。

---

## 類別 F：已 runtime 實測確認的已知問題（2026-06-28）

### F-1 GE Blueprint 資產未被 cook（已修）

**根本原因**：`UGasEffectRegistry::Initialize()` 用 `FString::Printf + StaticLoadClass` 構造執行期路徑，`MaterialRegistry` 用 macro 構造 `FSoftObjectPath` — 兩者都是 runtime string，cooker 無法追蹤 dependency graph，`Content/Abilities/` 和 `Content/PhysicalMaterials/` 整個目錄不會被 cook。

**修法（2026-06-28 已修）**：在 `Config/DefaultGame.ini` 加：
```ini
[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToAlwaysCook=(Path="/Game/Abilities")
+DirectoriesToAlwaysCook=(Path="/Game/PhysicalMaterials")
+DirectoriesToAlwaysCook=(Path="/Game/Data")
```

**preflight 13ah** 現在自動偵測同行 `LoadObject<T>(nullptr, TEXT("/Game/...")`)` 類型，並對照 `DefaultGame.ini` 的 `DirectoriesToAlwaysCook`。
跨行路徑構造（`FString::Printf → StaticLoadClass(nullptr, *Path)`）無法靜態偵測，需人工審查。

### C-9 SoftObjectPath / StaticLoadClass 在 Packaged 執行檔的可達性

**風險**：任何透過 runtime string 載入資產的位置，若目標目錄不在 `DirectoriesToAlwaysCook`，Packaged 版本必定失敗（Editor/PIE 因有 AssetRegistry 全量可見所以不報錯，給人「在 Editor 正常」的假象）。

**掃法**：
1. `preflight 13ah` 自動偵測同行路徑（PASS = 已涵蓋）
2. 手動審查多行路徑構造（`FString::Printf`/macro 拼字串→ LoadClass/LoadObject）
3. 確認 `DefaultGame.ini` `DirectoriesToAlwaysCook` 涵蓋所有這類目錄

---

*最後更新：2026-06-28（G 類 UI/移動語意陷阱 7 條新增：G-1 Widget lifecycle；G-2 HUD default Visible；G-3 地形 octaves greedy mesh；G-4 固定鏡頭 W/S 方向；G-5 K 鍵飛行 IsFalling 限制；G-6 生存條順序；G-7 MP CCW；preflight 13ai/13aj/13ak/13al/13am 新增自動偵測）*
