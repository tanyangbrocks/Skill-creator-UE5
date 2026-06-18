# NPC Brain 實作計畫

> 目標：任何生物 / NPC 皆可掛載 `UNPCBrainComponent`，擁有身分、記憶、個性、世界感知與對話能力。
> 推理後端：本地 llama-server（無雲端費用），可日後換更強模型而不改架構。

---

## 系統架構

```
[感知層]  世界事件 / 玩家輸入 / 場景快照
              ↓
[記憶層]  FNPCIdentity（身分） + 短期記憶 buffer + 長期記憶摘要
              ↓
[思考層]  FPromptBuilder → llama-server → FNPCResponseParser
              ↓
[行動層]  對話框 / BT 行動 / SpellRunner / VoxelWorld tile 操作
```

---

## 里程碑一覽

| 里程碑 | 名稱 | 狀態 |
|--------|------|------|
| M-NPC-0 | llama-server 推理基礎架構 | ✅ build 驗證通過（2026-06-19），llama-server.exe + Phi-3 模型已部署 |
| M-NPC-1 | 身分與個性系統 | ✅ |
| M-NPC-2 | 記憶系統 | ✅ |
| M-NPC-3 | 世界感知 | ✅（`IWorldInterface` 接通為待辦，見下方說明）|
| M-NPC-4 | Prompt 組裝 + 回應解析 | ⏳ |
| M-NPC-5 | 玩家對話介面 | ⏳ |
| M-NPC-6 | 行動系統 | ⏳ |
| M-NPC-7 | 自動批量 NPC 生成 | ⏳ |
| M-NPC-8 | Fine-tune 管線（選做，後期）| ⏳ |

---

## M-NPC-0：llama-server 推理基礎架構 ✅

**目標**：UE5 能對本地 llama-server 送出 prompt 並收到回應。

### 完成內容

| 檔案 | 說明 |
|------|------|
| `Plugins/NPCBrain/NPCBrain.uplugin` | Plugin 描述 |
| `Plugins/NPCBrain/Source/NPCBrain/NPCBrain.Build.cs` | 依賴：HTTP / Json / DeveloperSettings |
| `Public/NPCBrainTypes.h` | `FNPCMessage`、`ENPCMessageRole`、兩個 delegate 型別 |
| `Public/NPCBrainSettings.h` | `UNPCBrainSettings`（config=NPCBrain），Project Settings 可編輯 |
| `Public/LlamaServerProcess.h/.cpp` | `FPlatformProcess::CreateProc` 管理子程序，Launch / Shutdown / IsRunning |
| `Public/LlamaInferenceClient.h/.cpp` | POST `/v1/chat/completions`，lambda capture move-only TDelegate |
| `Public/UNPCBrainSubsystem.h/.cpp` | `UWorldSubsystem`，FTSTicker 輪詢 /health，BindWeakLambda 防 GC crash |
| `Config/DefaultNPCBrain.ini` | 預設路徑設定（使用者需修改） |

### ⚠️ 使用者需完成的手動步驟（Build 前）

#### 步驟 A：下載 llama-server.exe

1. 前往 **https://github.com/ggerganov/llama.cpp/releases**
2. 選最新 release，下載 Windows 版：
   - 無 NVIDIA GPU → `llama-b{版本號}-bin-win-cpu-x64.zip`
   - 有 NVIDIA GPU（建議）→ `llama-b{版本號}-bin-win-cuda12-x64.zip`
3. 解壓後整個資料夾放到：
   ```
   C:\SkillCreatorUE5\Tools\llama\
   ```
   確認路徑 `C:\SkillCreatorUE5\Tools\llama\llama-server.exe` 存在。

#### 步驟 B：下載 GGUF 模型

推薦起點：**Phi-3 Mini**（約 2.2 GB，品質與速度平衡佳）

1. 前往 HuggingFace，搜尋：`microsoft/Phi-3-mini-4k-instruct-gguf`
2. 下載：`Phi-3-mini-4k-instruct-q4.gguf`
3. 放到：
   ```
   C:\SkillCreatorUE5\Tools\llama\models\Phi-3-mini-4k-instruct-q4.gguf
   ```

> 若日後要換模型，只需更換 `DefaultNPCBrain.ini` 的 `ModelPath`，程式碼完全不動。

#### 步驟 C：Build + 驗證

1. 關閉 UE Editor
2. Visual Studio → Build `SkillCreatorUE5Editor`
3. 開啟 Editor → Play → 觀察 Output Log 是否出現：
   ```
   NPCBrain: llama-server launched (port 8080)
   NPCBrain: Server ready — LLM inference available
   ```

---

## M-NPC-1：身分與個性系統 ✅

**目標**：每個 NPC 有一份完整的靜態身分，序列化存檔，遊戲啟動時不重新生成。

### 計畫內容

- `FNPCIdentity` struct：`Name / Species / Faction / Role / Traits[] / SpeechStyle / Backstory`
- `UNPCIdentityGeneratorSubsystem`：用世界觀 system prompt 一次性生成，寫入 `.json` 存檔
- 世界觀 system prompt 注入點（從 `docs/plan-worldlore-integration.md` 取材）
- 生成後身分鎖定，不受 runtime 推理覆寫

### 完成內容（2026-06-19）

| 檔案 | 說明 |
|------|------|
| `Public/NPCIdentity.h/.cpp` | `FNPCIdentity` USTRUCT + `Save()/Load()`（`FJsonObjectConverter`，路徑 `{ProjectSavedDir}/NPCBrain/Identities/{NPCId}.json`，與 `FCharacterSaveData` 同慣例）|
| `Public/UNPCIdentityGeneratorSubsystem.h/.cpp` | `UWorldSubsystem`；`LoadOrGenerate(NPCId, OnReady)`：磁碟已有存檔 → 直接讀回（身分鎖定，永不重新生成）；否則呼叫 `UNPCBrainSubsystem::SendMessages` 帶 worldview system prompt，解析 LLM 回應 JSON 存檔 |
| `NPCBrainSettings.h/.cpp` | 新增 `WorldviewSystemPrompt`（config，注入點），預設值取材自 `docs/plan-worldlore-integration.md`（蒼究世界觀/境界體系），深度整合留給 W 系列 |

**已驗證**：JSON 存讀往返自動化測試（`NPCBrain.Identity.SaveLoadRoundTrip`）。
**未驗證（需實際 Play）**：LLM 生成路徑（要求 llama-server 真正回傳合法 JSON）。

---

## M-NPC-2：記憶系統 ✅

**目標**：NPC 記得發生過的事，但 context window 不會爆。

### 計畫內容

- **短期**：`TCircularBuffer<FNPCMemoryEvent>`，保留最近 20 條
- **長期**：每累積 20 條，送一次 LLM 壓縮成 1 段摘要，清空 buffer
- **重要事件**：`bPermanent = true` 標記，永遠保留（不被壓縮淘汰）
- `FNPCMemoryEvent`：`Timestamp / Category(Observe/Speak/Receive/World) / Content`

### 完成內容（2026-06-19）

| 檔案 | 說明 |
|------|------|
| `Public/NPCMemoryTypes.h` | `ENPCMemoryCategory`（Observe/Speak/Receive/World）+ `FNPCMemoryEvent`（Timestamp/Category/Content/bPermanent）|
| `Public/UNPCMemoryComponent.h/.cpp` | `UActorComponent`；`AddMemoryEvent()` 依 `bPermanent` 分流：永久事件進 `PermanentMemory`（永不淘汰），其餘進 `ShortTermMemory`（cap=20，滿了觸發 `UNPCBrainSubsystem::SendMessages` 壓縮成摘要併入 `LongTermSummary`，並清空緩衝區）；壓縮失敗時把該批事件退回緩衝區重試，不丟資料 |

**設計偏差**：計畫原文寫 `TCircularBuffer<FNPCMemoryEvent>`，實作改用 `TArray` + FIFO。20 筆上限下兩者效能差異可忽略，`TCircularBuffer` 的固定 2 的次方容量與索引迴繞管理在此沒有實際好處，改用更直觀的寫法。
**已驗證**：永久事件不被淘汰、未達上限不觸發壓縮、達上限觸發壓縮並清空緩衝區（無 `UNPCBrainSubsystem` 時走 fallback 路徑，串接原始事件文字，不丟資料）。
**未驗證（需實際 Play）**：真正呼叫 LLM 產生的摘要品質。

---

## M-NPC-3：世界感知 ✅

**目標**：NPC 能察覺周遭世界的變化並轉成自然語言。

### 計畫內容

- 訂閱 VoxelWorld tile change event（接現有 CA 事件系統）
- 訂閱附近 Character spawn / death / state change
- `FWorldSnapshot`：將感知到的事件轉成一段描述文字（e.g. "附近 5 格有火焰蔓延"）
- `PerceptionRadius` 可按生物種類設定（野獸感知半徑 vs 人類）
- 感知事件自動寫入短期記憶

### 完成內容（2026-06-19）

| 檔案 | 說明 |
|------|------|
| `Public/WorldSnapshot.h/.cpp` | `FWorldSnapshot`（`NearbyCreatureIds` + `HazardMaterials`，純資料，無 `UWorld` 依賴）+ `FNPCPerceptionLogic`（`IsHazardMaterial` / `DescribeChanges`，純邏輯，可離線單元測試）|
| `Public/UNPCPerceptionComponent.h/.cpp` | `UActorComponent`；`TickPerception(DeltaTime, OwnerPos)` 由擁有者手動呼叫（沿用 SpellRunner 手動累加器慣例，非引擎 `TickComponent`）；每 `PerceptionIntervalSeconds` 掃描一次半徑內生物（`IWorldInterface::GetEntitiesNear`）與危險材質（熔岩/火焰/蒸氣，Chebyshev 距離），與上次快照差異化後寫入同 Actor 上的 `UNPCMemoryComponent`（World 分類）|
| `NPCBrain.Build.cs` | 新增 `SkillCreatorCore` 依賴（取得 `FGridPos`/`EMaterialType`/`ICreature`/`IWorldInterface`，刻意不依賴 `SkillCreatorRuntime`/`VoxelWorld`，保持 NPCBrain 與具體 Actor 型別解耦）|

**架構決策**：未直接耦合 `AEnemy`/`AEnemyManager`/`TileWorld3D`，改用專案既有但目前**零實作**的 `IWorldInterface`（`SkillCreatorCore/Public/IWorldInterface.h`，原為 SpellCaster 設計但從未接通）作為依賴注入點。`UNPCPerceptionComponent::SetWorldInterface()` 在沒人呼叫前是安全的 no-op。
**⚠️ 待辦（非本次範圍）**：要讓感知系統真正運作，需要某處實作 `IWorldInterface`（橋接 `AVoxelWorldActor` + `AEnemyManager`）並呼叫 `SetWorldInterface()` + 每幀呼叫 `TickPerception()`。這是既有的架構缺口，不是這次新增的技術債——建議在 M-NPC-5/6（真正接遊戲玩法）時一併處理。
**已驗證**：`FNPCPerceptionLogic::DescribeChanges` 的生物進出/危險材質出現消退判斷、`IsHazardMaterial` 分類正確性。
**未驗證（需 `IWorldInterface` 接通後才能測）**：`UNPCPerceptionComponent` 實際掃描真實世界的行為。

---

## M-NPC-4：Prompt 組裝 + 回應解析 ⏳

**目標**：把所有資訊組成一個完整的 prompt，並正確解析模型輸出。

### 計畫內容

- `FPromptBuilder`：`世界觀 system + 身分 + 長期記憶摘要 + 短期記憶 + 當前場景快照` → 一次 prompt
- Chain-of-thought 格式：`<think>…</think>\n<response>…</response>`（思考過程玩家看不到）
- `FNPCResponseParser`：解析出 `dialogue / action / emotion / memory_note`
- `memory_note` 自動寫入短期記憶

### 回應 JSON 格式（期望模型輸出）

```json
{
  "dialogue": "你這個外鄉人，來這裡做什麼？",
  "action": "Idle",
  "emotion": "Suspicious",
  "memory_note": "一名外鄉人來到了村莊"
}
```

---

## M-NPC-5：玩家對話介面 ⏳

**目標**：玩家能打字跟 NPC 說話，NPC 回應顯示成對話框。

### 計畫內容

- 接近 NPC 後顯示文字輸入框（接現有 Widget 系統）
- 玩家輸入 → `UNPCBrainSubsystem::SendMessages()` → 回應顯示
- 非對話觸發：攻擊、接近（進入半徑）、目睹世界事件，也能觸發 NPC 主動反應
- 輸入框顯示「NPC 正在思考…」動畫（避免 1–3 秒延遲感）

---

## M-NPC-6：行動系統 ⏳

**目標**：LLM 輸出的 action 真的觸發遊戲行為。

### 行動對照表

| LLM 輸出 `action` | 觸發 |
|-------------------|------|
| `Idle` | 維持當前狀態 |
| `Attack` | BT Task → 現有攻擊系統 |
| `Flee` | BT Task → 逃跑邏輯 |
| `Follow` | BT Task → 跟隨目標 |
| `Trade` | 開啟交易 UI |
| `CastSpell` | 接現有 SpellRunner |
| `PlaceTile` / `BreakTile` | 呼叫 VoxelWorld API |
| `GiveItem` | 呼叫 Inventory 系統 |

---

## M-NPC-7：自動批量 NPC 生成 ⏳

**目標**：輸入地區 + 種族 + 條件，輸出一整批各不相同的 NPC 身分。

### 計畫內容

- 輸入：`Region / Species / SocialContext / Count`
- LLM 生成 N 個 `FNPCIdentity`，確保名字、職業、個性不重複
- 批量寫入存檔，後續遊戲啟動直接讀取（不重新生成）
- 可用 Editor 工具或命令觸發（`UNPCGeneratorSubsystem::GenerateBatch()`）

---

## M-NPC-8：Fine-tune 管線（選做，後期）⏳

**目標**：蒐集測試期的優質對話，Fine-tune 模型使其更符合世界觀語氣。

### 計畫內容

- 對話品質評分系統（玩家或開發者標記好/壞回應）
- 蒐集到足量資料後，使用 `llama.cpp` 的 `finetune` 工具或外部平台（Axolotl / Unsloth）
- Fine-tune 產出新的 `.gguf`，替換 `ModelPath` 即完成升級

---

## 技術備註

| 項目 | 決策 |
|------|------|
| 推理後端 | llama-server（HTTP，OpenAI-compatible API） |
| 換模型成本 | 改 `DefaultNPCBrain.ini` 的 `ModelPath`，程式碼不動 |
| 記憶 context 策略 | 短期 buffer + LLM 壓縮長期摘要 |
| 對話延遲 | 1–3 秒（顯示思考動畫緩衝） |
| 離線支援 | ✅ 完全本地，不需網路 |
| 模型大小 | Phi-3 Mini Q4 ≈ 2.2 GB（建議起點） |
| GPU 加速 | CPU 可用；下載 CUDA 版 llama-server 可提升至近即時 |
