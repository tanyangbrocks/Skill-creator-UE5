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

*最後更新：2026-06-28（A-1~A-4 全掃修復 A-3；B-1 掃描修復 SurfaceWaterPool WaterFill 符號錯誤；B-2 掃描 delegate 全部正常；GAS-0~6 整合；D-1~D-3 GAS Loose Tag 陷阱新增）*
