# M-10｜GPU Compute Shader CA 模擬 實作計畫

> 目標：Grain 32/64+ 世界的 Powder/Liquid CA 模擬達到可接受幀率。
> Godot 參照：`C:\skill-creator\Scripts\World\CaGpuSimulator.cs`（469 行，**完整、能跑、已驗證的
> 參照實作**，不是設計草稿）+ `TileWorld3D.cs` CPU CA 邏輯。本文件逐行讀過上述檔案後撰寫。
> UE5 現狀：`docs/plan-ue5-migration.md` §M-10 已有架構決策（雙軌制），`FCaGpuSimulator`/
> `FTileWorld3D` 已有完整 stub 骨架——**這不是從零開始的任務，是把已經設計好、甚至連呼叫端都
> 接好的骨架填入真正的 RDG 實作**。

---

## 一、現狀盤點（先搞清楚已經有什麼，不要重做）

讀完 UE5 程式碼後，比之前粗估的更樂觀——骨架比想像中完整：

| 項目 | 檔案 | 狀態 |
|------|------|------|
| `ETileCategory`（GameplayBlock/VisualCA 雙軌制分類） | `MaterialType.h:7-16` | ✅ enum 已定義，`FTileCell::Category`（第 49 行）已預留 byte。**但目前沒有任何地方真的填值**（見下方§五決策 1） |
| `EPhysicsCategory`（Static/Powder/Liquid/Gas）+ Density | `MaterialRegistry.h:9,21-22` | ✅ 已存在，直接對應 Godot `MaterialData.Physics`/`.Density`，`FMaterialRegistry::GetPhysics(MaterialID)` 已有 accessor |
| `FCaGpuSimulator` stub | `CaGpuSimulator.h`（全檔） | ✅ 公開 API 骨架完整（`Initialize/IsAvailable/SetOrigin/Upload/Simulate/Download/Release`），`ZoneW/H/D=128/256/128` 常數已定義。**全部方法目前是 no-op**，這是本計畫要填的核心 |
| `FTileWorld3D` GPU 串接點 | `TileWorld3D.h:48-53,93` + `.cpp:25-46` | ✅ `GpuSim` 成員、`InitGpu()`、`UpdateGpuOrigin()`、`InGpuZone()`、`SetCellFromGpu()` 全部已寫好並可呼叫（`SetCellFromGpu` 內部呼叫既有的 `SetTile()`）。**但 `InGpuZone()` 目前沒有任何呼叫端**（搜尋整個 `.cpp` 只有自己的定義），`Tick()` 也完全沒有呼叫 `GpuSim.Upload/Simulate/Download` |
| Chunk 尺寸對應 | `WorldScale::ChunkSize=16` | ✅ 與 Godot `Chunk3D.Size=16` 一致（`CaGpuSimulator.cs:149` 用同一個常數做 chunk-based 上傳） |

**結論**：真正要做的事，幾乎全部集中在 `FCaGpuSimulator.cpp`（目前根本不存在這個檔案，只有 `.h`）內部，加上 `FTileWorld3D::Tick()` 補一段呼叫邏輯。`ETileCategory` 分類、雙軌制架構決策、Chunk 對應，這些「設計層」的事前都已經做完了。

---

## 二、Godot 參照演算法精讀（逐行對照，不是憑印象）

### 2.1 不是「全格網平行掃描」，是 Margolus 2×2×2 Block CA

`CaGpuSimulator.cs:295` 每個 GPU invocation 負責一個 2×2×2 block（workgroup = 4×4×4 invocation = 64 block = 8×8×8 格）。

**為什麼不能直接把 CPU 的逐格 `TryMove` 搬上 GPU**：UE5 現有 CPU 演算法（`TileWorld3D.cpp:339-380` `UpdatePowder`/`UpdateLiquid`）是「依序掃描每一格，找到就立刻 swap」——這在單執行緒上安全，但平行執行（GPU）會造成多個 thread 同時讀寫鄰格的競爭（A 格讀 B 格決定要不要移過去，同時 B 格也在讀 A 格）。Margolus block CA 用「每個 invocation 只碰自己那顆 2×2×2 block 的 8 格」解決：同一個 block 內部用區域陣列 `c[2][2][2]` 暫存、運算、寫回，跟其他 block 完全不重疊讀寫。

**Phase 0/1 棋盤格偏移**（`CaGpuSimulator.cs:353-362`，GLSL `main()` 開頭）：
```glsl
int bx = gid.x * 2 + pc.phase;   // phase=0 或 1，整個 Simulate() 兩個 phase 各 dispatch 一次
```
每次 `Simulate()` 呼叫，兩個 phase 各跑一次完整 dispatch（`CaGpuSimulator.cs:217-230` 的 `for (int phase = 0; phase < 2; phase++)`）——這樣 block 邊界每次偏移半格，讓格子能跨越「上一輪 block 邊界」移動，避免被困在固定網格線內。**這是整個演算法最容易翻譯錯的地方**：phase 偏移錯一格，CA 會出現「格子卡在隱形網格上不會跨界移動」的視覺 bug，肉眼很難一眼看出問題在 phase 計算還是別的地方。

### 2.2 Block 內部兩個 Pass（GLSL `main()` 主體，`CaGpuSimulator.cs:391-455`）

**Pass 1（重力，第 391-424 行）**：對 block 內 2×2 的 XZ 位置（用 Fisher-Yates shuffle 決定處理順序，第 379-389 行，避免固定順序造成的方向偏斜），檢查頂層格 `c[lx][0][lz]` 能不能直接落到底層 `c[lx][1][lz]`（垂直），不行的話 Powder 再試兩個對角（`c[dlx][1][lz]` / `c[lx][1][dlz]`）。`canDisplace()`（第 330-334 行）的規則：目標格是空的，或目標跟自己都是液體且自己密度較高。

**Pass 2（液體橫向擴散，第 426-455 行）**：只對「這幀重力階段沒被換過」的液體格（`c[lx][1][lz] != orig[lx][1][lz]` 就跳過，第 437 行）做橫向擴散，**且只在 block 內部 2×2 範圍移動一格**——不是 CPU 版那種「Water 一次擴散 3 格、Lava 1 格」的多格掃描。長距離擴散是靠**多次 Simulate() 呼叫累積**自然產生的（CPU 版一幀內直接掃 3 格，GPU 版要 3 幀才會看到等效的擴散距離）。**這代表 GPU CA 跑起來的視覺速度會比 CPU 版慢，需要實測調整 `Simulate()` 每幀呼叫幾次來補回速度感，不能假設兩者天生等速。**

### 2.3 邊界外一律當固體（第 344-349 行）
```glsl
const uint SOLID_CELL = 1u;   // matType=1, physCompact=0 → 不可移動、不可被取代
uint readCell(...) { return inBounds(...) ? cells[...] : SOLID_CELL; }
```
Zone 邊界外讀到的鄰格視為不透明靜態。UE5 port 必須照搬，否則液體會在 zone 邊界「漏出去」消失。

### 2.4 Cell 打包格式（`CaGpuSimulator.cs:40-49,73-84`）
```
bits  0- 7 : MaterialType
bits  8- 9 : physCompact（0=靜態/空, 1=粉末, 2=液體）—— 來自 MaterialRegistry.Physics
bits 10-13 : density4（0-15，從 MaterialData.Density×1.5 換算）
bit   14   : dirty flag（shader 移動格子時設置，Download 只讀有設這個 bit 的格子）
bits 16-23 : Timer（CPU 維護，GPU 原封不動搬運，不參與運算）
bits 24-30 : Variant（同上）
```
`_physBits[]`（第 52-69 行）是**靜態建構時**算好的「MaterialType → (phys<<8)|(density<<10)」查表，Pack() 時直接查表組裝，不用每次重算。UE5 對應：`FMaterialRegistry::GetPhysics(MaterialID)` + `Density` 已存在，可以做一樣的靜態查表（`TArray<uint32> GPhysBits` 在某個 `static` 初始化區塊算一次）。

### 2.5 ⚠️ 全文最重要的發現：Godot 版本是「同步阻塞」，不是真正 async

`CaGpuSimulator.cs:228-229`：
```csharp
_rd.Submit();
_rd.Sync();   // ← 阻塞等待 GPU 完成，不是 async readback
```
`Simulate()` 每個 phase dispatch 完都直接 `Submit()+Sync()`，**整個函式是同步、會卡住呼叫端（遊戲主執行緒）直到 GPU 算完**。Godot 沒有做真正的 async readback——`Download()`（第 242-267 行）也是同步呼叫 `_rd.BufferGetData()`，當場就拿到資料。

這跟 `docs/plan-ue5-migration.md:646` 寫的「Simulation 結果 GPU → CPU（**async，非同步**）」**不一樣**——UE5 的既有計畫文件想做的比 Godot 參照實作更進階（真正 async fence-based readback，不卡 render thread）。這代表：

**Godot 的同步版本是一個現成、低風險、已驗證正確的「Phase 0」目標**——先把這個原封不動搬過來，能正確跑出視覺正確的 CA 結果，再考慮要不要做 UE5 計畫文件設想的進階 async 版本。**不要一開始就同時挑戰「翻譯演算法」+「自己設計從未驗證過的 async fence 時序」兩個風險，會很難排查是哪一層出的問題。**

### 2.6 Sparse 上傳（跳過 Air chunk，`CaGpuSimulator.cs:145-188`）
逐 chunk（16³）檢查，`Chunk3D` 不存在（= 全 Air）的整個 chunk 直接 `_staging.AsSpan(rowIdx, S).Clear()` 跳過逐格運算；存在的 chunk 才逐格 Pack。`Upload()` 回傳 `hasPhysics`（第 183 行）——zone 內完全沒有 Powder/Liquid 時，呼叫端可以**整個跳過 Simulate+Download**（`bool hasPhysics` 由 `(packed & PhysMask) != 0` 在上傳時順便算出，不用額外掃描）。這就是「Sparse CA」在 Godot 版的實際做法——**不是把 buffer 也做成稀疏結構，是「全空就不 dispatch」這麼簡單**，沒有像稽核報告講的「Option 2：把非空格打包進變動長度 buffer」那麼複雜。UE5 port 應該先抄這個簡單版本，不要自己發明更複雜的稀疏結構。

---

## 三、UE5 RDG 對應關係（Godot RenderingDevice ↔ UE5 RDG）

專案目前**沒有任何既有 compute shader**（搜尋 `FRDGBuilder`/`.usf`/`DECLARE_GLOBAL_SHADER` 全部找不到），這塊是真的從零開始，沒有專案內參照寫法可抄。

| Godot | UE5 對應 | 備註 |
|-------|---------|------|
| `RenderingDevice` | `FRHICommandListImmediate` + `FRDGBuilder` | UE5 沒有 Godot 那種「拿到一個 device 物件直接呼叫方法」的簡單介面，是透過 RDG 在每幀 render graph 裡建 Pass |
| GLSL `#version 450` 內嵌字串 + `ShaderCompileSpirVFromSource` | `.usf` HLSL 檔 + `DECLARE_GLOBAL_SHADER`/`IMPLEMENT_GLOBAL_SHADER` + Shader Permutation | UE5 shader 要編譯時期就存在的 `.usf` 檔，不能像 Godot 那樣 runtime 字串編譯（at least 不是慣用做法） |
| `StorageBufferCreate` + `RDUniform`(binding=0) | `FRDGBuilder::CreateBuffer` + `FRDGBuilder::CreateUAV` + Shader Parameter Struct（`SHADER_PARAMETER_RDG_BUFFER_UAV`） | |
| Push Constant（32 bytes） | Shader Parameter Struct 的一般欄位（`SHADER_PARAMETER(int32, W)` 等） | RDG 用 Uniform Buffer 取代 push constant 概念，但效果類似 |
| `ComputeListDispatch(gx,gy,gz)` | `GraphBuilder::AddPass(...)`（通常配 `FComputeShaderUtils::AddPass` helper） | |
| `BufferUpdate`（CPU→GPU 上傳） | `GraphBuilder.QueueBufferUpload()` 或透過 `FRDGAsyncScatterUploadBuffer` | |
| `BufferGetData`（同步讀回） | `GraphBuilder::QueueBufferExtraction(Buffer, ..., ERHIAccess::CPURead)` 接著 `GraphBuilder::Execute()` | 2026-06-21 查證：Epic 官方教學《[Simple compute shader with CPU readback](https://dev.epicgames.com/community/learning/tutorials/WkwJ/unreal-engine-simple-compute-shader-with-cpu-readback)》就是走這條 `CreateBuffer→CreateUAV→AddPass→QueueBufferExtraction(CPURead)→Execute()` 路徑，是 Phase 0 的具體參照範本，不用自己從 RDG 文件東拼西湊。⚠️ 但 RDG 整個設計就是「圖建好才一次執行」，跟 Godot `Submit()+Sync()` 那種「呼叫當下就阻塞等結果」的心智模型不完全對等——`QueueBufferExtraction` 拿到的 `TRefCountPtr<FRDGPooledBuffer>` 何時真正有資料、要不要額外 `FlushRenderingCommands()` 才能在同一個 game thread 呼叫內安全讀取，是 Phase 0 spike 本身要去確認的事，不能想當然耳 |

### 3.1 可以省一部分管線苦工的現成插件：CompushadyUnreal

[rdeioris/CompushadyUnreal](https://github.com/rdeioris/CompushadyUnreal)（**MIT 授權，持續維護中**——2026-06-21 查證 star 數 62、最後更新 2026-06-19、未封存）把上面那張表格右邊那堆 RDG 樣板包成一套統一 API：執行期載入/編譯 HLSL 或 GLSL 字串、自動處理 buffer/texture 的 UAV/SRV 綁定、`DispatchSync`（同步）/`DispatchByMapAndProfile`（非同步，可量測耗時）等現成 dispatch 函式。

⚠️ **跟本專案慣例的落差**：讀過 README 的 Quickstart 後發現，**這個插件主要的、寫得完整的使用方式是 Level Blueprint 節點**（`MakeHLSLString`/`CreateCompushadyUAVTexture2D`/`DispatchByMapSync` 等全部是 BP node，README step1~step3 的範例整個流程都在 Blueprint 裡完成）。Quickstart 大綱裡列了一個「step9, C++」章節，暗示底層有原生 C++ API（BP node 應該只是包裝），但這次查證時這個章節內容是空的，沒辦法確認 C++ API 的實際長相。

這跟 CLAUDE.md「最小化使用者手動 Editor 操作，能用 C++ 解決的不要用 Blueprint」的專案鐵則有衝突——`FTileWorld3D`/`FCaGpuSimulator` 整套都是純 C++、不依賴任何 Blueprint 資產，如果 CompushadyUnreal 的 C++ API 不夠完整或不夠穩定，硬套會破壞這個一致性。**建議**：真正要採用前，先花一兩個小時實際讀它的 C++ 原始碼（不是 README），確認 C++ 介面好不好用、夠不夠完整，再決定是「拿來省 Phase 0/2 的管線苦工」還是「照 Epic 官方教學自己手刻 `FRDGBuilder`」。兩條路都可行，不是非此即不可，差別只在工作量跟對這個插件的依賴程度。

---

## 四、建議實作階段（刻意把「翻譯演算法」跟「做成真正 async」拆成兩個階段，降低風險）

### Phase 0：最小驗證 spike（不含真正 CA 邏輯）✅ 2026-06-21 完成並通過

實際走的是「手刻 RDG」路線（沒有用 CompushadyUnreal，理由見 §3.1，純 C++ 跟專案慣例一致）。
新增 `Plugins/VoxelWorld/Source/VoxelWorldShaders/`（獨立模組，理由見下方「踩到的坑」第 1 點）+
`Plugins/VoxelWorld/Shaders/Private/CaSpike.usf`，自動化測試
`VoxelWorldShaders.M10.GpuSpike` 跑出 `Result={Success}`：

```
UnrealEditor-Cmd.exe "<uproject>" -ExecCmds="Automation RunTests VoxelWorldShaders.M10.GpuSpike; Quit" -unattended -nopause -log
```

⚠️ **這個測試需要真正的 RHI（D3D12/Vulkan），不能用專案既有 automation test 慣例的
`-nullrhi`**（`SkillCreatorCoreTests.cpp` 開頭文件範例的 CI 跑法），NullRHI 沒有真正的 GPU
資源，compute shader 測試在那種模式下不會有意義。

**踩到的坑（依出現順序，Phase 1/2 寫真正的 Margolus shader 時會原樣再撞到，先記下來）**：

1. **Shader type 註冊得太晚** → `Assertion failed: !AreShaderTypesInitialized()`。
   `IMPLEMENT_GLOBAL_SHADER` 在模組 DLL 載入當下做靜態註冊，但主要的 `VoxelWorld` 模組是
   `LoadingPhase=Default`，這時候引擎已經把 shader type 註冊表鎖住。**解法**：shader 類別
   獨立拆到一個新模組 `VoxelWorldShaders`（只依賴 `Core`/`RenderCore`/`RHI`/`Projects`，
   這些本來就載入得很早，避免依賴 `Default` 階段的模組造成載入順序矛盾），`.uplugin` 設
   `LoadingPhase=PostConfigInit`。**這代表 Phase 1 的真正 Margolus shader 類別也要放在
   `VoxelWorldShaders`，不能放回主要的 `VoxelWorld` 模組。**
2. **Shader 虛擬路徑漏掉 `Private` 子資料夾** → `Fatal error: Couldn't find source file`。
   `.usf` 檔實際放在 `Shaders/Private/CaSpike.usf`，但 `AddShaderSourceDirectoryMapping`
   只把 `Shaders/` 對應到虛擬根目錄，`Private` 仍是路徑的一部分——`IMPLEMENT_GLOBAL_SHADER`
   要寫成 `"/Plugin/VoxelWorld/Private/CaSpike.usf"`，不是 `".../CaSpike.usf"`。
3. **`.usf` 缺少強制 include** → `Error: Shader is required to include /Engine/Public/Platform.ush`。
   每個 UE5 shader 檔案開頭都要加這行，沒有例外。
4. **`FRHIGPUBufferReadback` 不能在 game thread 建構** → `Assertion failed: IsInRenderingThread()`。
   建構子內部呼叫 `RHICreateGPUFence()`，要求 render thread。要在 `ENQUEUE_RENDER_COMMAND`
   的 lambda 裡面 `new`，透過參照寫回 game thread 的變數，`FlushRenderingCommands()` 後讀取。
5. **`Lock()`/`Unlock()` 一樣要求 render thread** → 同一個 assert 又跳一次，這次是
   `FRHIGPUBufferReadback::Lock()` 內部呼叫的 `LockStagingBuffer_RenderThread()`
   （函式名稱本身就講明白了）。讀回資料、驗證、`Unlock()`、`delete` 整段都要包進**第二個**
   `ENQUEUE_RENDER_COMMAND`，结果透過參照寫回再 `FlushRenderingCommands()`。
   `Poll()`/`IsReady()` 例外——文件明確只說 `Wait()` 要 render thread，`Poll()` 沒有這個限制，
   可以留在 game thread 忙等迴圈裡（但官方文件警告不要無止盡忙等，見下一點）。
6. **`FRHIGPUFence::Poll()` 的文件警告**（還沒在這次 spike 撞到，但 Phase 5 async 升級時
   要注意）：「不要用 Poll() 在迴圈裡忙等到完成——有些平台的 RHI 不會 signal fence，除非
   RHI thread 持續在推進、送出 GPU 工作」。這次 spike 用的 0.001s sleep 忙等迴圈能跑通，
   是因為只有單次 dispatch、馬上有別的工作（`Automation` 框架本身）讓 RHI thread 繼續推進；
   等到 Phase 3 真正每幀呼叫 `Upload→Simulate→Download`，要重新評估這個忙等模式是否
   仍然安全，或者要不要提前做 Phase 5 的真正 async（不阻塞 game thread）。

通過標準：CPU 端能正確讀到 GPU 算完「index*2」的結果（256 格全部正確）。✅ 已達成。

### Phase 1：Margolus CA shader 邏輯逐行 port（HLSL）✅ 2026-06-21 完成並通過

實際做法跟原計畫一致：`canDisplace`/`readCell`/Pass 1/Pass 2/dirty flag 回寫逐行對照 GLSL
翻成 HLSL（`Plugins/VoxelWorld/Shaders/Private/CaMargolus.usf` + `VoxelWorldShaders/Public/CaMargolusShader.h`），
Pack/Unpack 用 `FMaterialRegistry::GetPhysics()`/`Density` 即時查（不另建靜態查表，材質不多
成本可忽略，見 `VoxelWorld/Public/CaCellPacking.h`——**這個檔案放在 `VoxelWorld` 模組，不是
`VoxelWorldShaders`**，因為它需要 `FMaterialRegistry`，而 `VoxelWorldShaders` 不能依賴
`VoxelWorld`（會循環依賴，`VoxelWorld` 已經依賴 `VoxelWorldShaders`）——`VoxelWorldShaders`
只管「給一段 raw uint32 buffer，照 Margolus 規則搬動」，完全不知道遊戲材質是什麼，這個職責
切分是刻意的，不是妥協）。

驗證沒有照原計畫做「9×9×9 整區跟 CPU 比對最終靜止狀態」（那種比對既不確定性、也不好排查
是哪裡翻譯錯），改成更直接的單元測試：4×4×4 全 Air 的 zone，(0,0,0) 放一顆 Sand，dispatch
一次完整 Simulate（Phase 0+1），驗證 Sand 確實落到 (0,1,0)——這個案例刻意設計成跟亂數處理
順序無關（只有一個 mobile cell，沒有競爭對象），結果是確定性的。自動化測試
`VoxelWorld.M10.MargolusGravity` 跑出 `Result={Success}`。

**踩到的新坑**（跟 Phase 0 那 5 個不同，這個是 HLSL/shader-parameter 層級的）：

6. **C++ `SHADER_PARAMETER` 跟 `.usf` 全域變數對不上** → `Assertion failed: !Member.bIsBindable`
   （`ShaderCompilerCommon.cpp:828`，在 `AddUnboundShaderParameterError()` 函式內部——這個
   函式本身是「回報參數對不上的錯誤」，但回報的過程自己又斷言失敗，導致整個
   ShaderCompileWorker 直接崩潰，看不到真正有用的錯誤訊息，只能從崩潰位置反推）。
   **真正原因**：`.usf` 裡寫了 `int W, H, D;` 這種逗號分隔的單行多變數宣告，UE5 的
   shader parameter 解析器似乎沒辦法正確解析這種寫法（跟 C++ `SHADER_PARAMETER(int32, W)`
   等逐個宣告對不起來）。**拆成一行一個變數宣告就解決**，跟 Pass 0 的 spike shader
   寫法（本來就是一行一個）保持一致。
   ⚠️ 中途走過一條死路：一開始懷疑是 `uint c[2][2][2]` 三維區域陣列觸發 DXC 編譯器內部
   bug，改成扁平 `uint c[8]` + `LocalIdx()` 手動算 index 重試，**結果一模一樣的 assert
   還是跳，證明三維陣列不是真正原因**——這提醒之後遇到同類錯誤，先檢查 shader parameter
   宣告本身對不對，不要急著懷疑陣列維度這種比較罕見、比較難驗證的猜測。
7. **跨模組呼叫 shader 類別要加匯出宏**：`FCaMargolusCS` 沒加 `VOXELWORLDSHADERS_API`，
   `VoxelWorld` 模組（呼叫端 `M10MargolusTest.cpp`）連結時找不到 `GetStaticType()` 等符號
   （`LNK2019 無法解析的外部符號`）。Phase 0 的 `FCaSpikeCS` 沒加這個宏沒事，是因為當時
   呼叫端跟類別宣告剛好在同一個模組（`VoxelWorldShaders`）——這次呼叫端在不同模組才踩到。
   以後任何要被其他模組用 `TShaderMapRef<>` 引用的 shader 類別都要記得加。
8. **同模組內兩個 `.cpp` 同名**（`Private/M10MargolusTest.cpp` vs `Private/Tests/M10MargolusTest.cpp`）
   → UBT 直接報錯「Multiple input files found with duplicate filenames」，跟子資料夾無關，
   重新命名其中一個即可。

### Phase 2：RDG 管線（同步版，先不做 async）✅ 2026-06-21 完成並通過

`FCaGpuSimulator::Initialize/Upload/Simulate/Download` 填了真正實作（`CaGpuSimulator.cpp`），
`Download()` 同步阻塞讀回（對應 Godot `Submit()+Sync()`）。原本的 stub 簽名
`Upload(const TArray<uint8>&)`/`Simulate()`（無參數）跟真實 Godot 演算法格式不符（純
MaterialID byte 不夠，shader 需要 physCompact/density 才能判斷移動規則）——確認沒有任何
呼叫端用到舊簽名後直接改成 `Upload(const TArray<uint32>& PackedCells, int32 W, int32 H, int32 D)`
（打包格式）+ `Simulate(uint32 RngSeed)`。

**關鍵設計**：Upload→Simulate→Download 是三次分開的公開 API 呼叫，但底層 GPU buffer 要
跨呼叫持續存在（不能每次都重建）——RDG 的 `FRDGBufferRef` 本質是單個 `FRDGBuilder::Execute()`
範圍內的暫時資源，跨呼叫持久化要靠 `TRefCountPtr<FRDGPooledBuffer>`：`Upload()` 結束時
`QueueBufferExtraction()` 轉成 pooled buffer 存到成員變數，`Simulate()`/`Download()` 開頭用
`RegisterExternalBuffer()` 接回來，`Simulate()` 結束前再 `QueueBufferExtraction()` 一次保持
最新狀態。

驗證用跟 Phase 1 同一個重力測試案例，但這次走真正的公開 API（`M10GpuSimulatorTest.cpp`），
證明三次分開呼叫之間的 buffer 持久化機制真的接好，不是只測單次 dispatch。自動化測試
`VoxelWorld.M10.GpuSimulatorGravity` 跑出 `Result={Success}`。

**踩到的坑**：兩個 `.cpp`（`M10MargolusTest.cpp` 跟新的 `CaGpuSimulator.cpp`）各自定義了
同名 `static int32 ComputeMargolusGroups(int32)`，編譯報錯「函式主體已經被宣告了」——這個
專案用 **Unity Build**（整個模組的 `.cpp` 合併成一個翻譯單元編譯，build log 顯示
`Compile [x64] Module.VoxelWorld.cpp`），「同檔案內 `static` = 內部連結、不同檔案不會撞名」
這個 C++ 慣常認知在 Unity Build 下失效。改名其中一個（加 `_Phase1Test` 後綴）解決。
⚠️ 提醒之後任何新增的 `static` helper 函式：在這個專案裡，函式名稱要當作「整個模組共用
的命名空間」來想，不能只看單一檔案內是否唯一。

### Phase 3：串接到 `FTileWorld3D::Tick()`✅ 2026-06-21 程式碼完成並 Build 通過，⚠️ PIE 視覺驗證待補

`Tick()` 開頭補上 GPU 路徑（`TileWorld3D.cpp`）：`GpuSim.IsAvailable()` 成立時，先用
`ActiveCX/CY/CZ`（呼叫端已經算好的玩家 chunk 座標，見 `AVoxelWorldActor.cpp` 的
`PCX/PCY/PCZ`）換算出 chunk 中心世界座標餵給 `UpdateGpuOrigin()`，再呼叫新增的私有方法
`TickGpuZone()`（把 zone 內現有 chunk 資料打包成扁平 `uint32` 陣列、Upload→Simulate→Download，
Download 回呼用 `SetCellFromGpu()` 寫回）。**「GPU zone 跟著誰移動」這個原本列為待決策的
問題，其實早就在 `UpdateGpuOrigin()` 自己的 doc comment 寫死了**（"移動 GPU 模擬區中心到
玩家所在格"）——讀程式碼就解決，不是真的需要使用者決策。

**終於用上 `InGpuZone()`**：CPU 主迴圈的 `switch (FMaterialRegistry::GetPhysics(...))`
裡，`Powder`/`Liquid` 分支現在先檢查 `!InGpuZone(wx,wy,wz)` 才呼叫 CPU 版
`UpdatePowder`/`UpdateLiquid`，避免同一格被 CPU/GPU 各算一次互相打架。`Gas`/`Static`
維持全 CPU（GPU shader 只做 Margolus 重力，不處理火/蒸汽/燃燒化學反應）。

`SetCellFromGpu()` 簽名跟著 Phase 2 的 `Download()` 回呼改成 `uint32 PackedCell`（不再是
`uint8 MaterialByte`），用 `CaCellPacking::Unpack*()` 還原 `MaterialID`/`CA_State`/`Variant`，
**保留現有 `Category` 欄位不動**——GPU shader 完全不知道 `ETileCategory`，且全專案目前
也還沒有任何地方真的填這個欄位（見上方§五決策 1），維持現狀。

`AVoxelWorldActor::BeginPlay()` 加一行 `TileWorld.InitGpu()`（世界建立後初始化一次，
doc comment 早就寫好「在世界建立後呼叫一次」）——這是整個 GPU CA 路徑唯一的啟用開關，
RHI 不可用時 `IsAvailable()` 回 false，`Tick()` 整段跳過，照舊全 CPU，不會出錯。

驗證：兩個 target（Editor/Game）Build 都是 0 錯誤 0 警告；重跑 Phase 0/1/2 的三個自動化
測試（`VoxelWorldShaders.M10.GpuSpike`/`VoxelWorld.M10.MargolusGravity`/
`VoxelWorld.M10.GpuSimulatorGravity`）+ 既有 4 個 `VoxelWorld.Preflight.*` CPU CA 測試
全部 `Result={Success}`，確認沒有回歸。

**這個 Phase 跟前面三個不一樣的地方**：自己寫的通過標準明確要求「實機 PIE 測試，玩家
附近的沙/水正常落下、流動，CPU 區域跟 GPU 區域交界處沒有明顯的行為斷層」——這是視覺/
質感判斷，自動化測試覆蓋不到（自動化測試只能驗證「沒有 crash、沒有明顯數值錯誤」這種
窄案例，驗證不了「玩起來順不順、邊界有沒有肉眼可見的斷層」）。**這部分需要使用者在
PIE 裡實際走到沙地/水邊看一次**，才能算這個 Phase 真正通過。

### Phase 4：效能驗證（Grain=16 先測，不要直接跳 32）
- 量測：每幀 `Upload+Simulate+Download` 實際耗時，跟現有 CPU CA 比較
- 如果同步阻塞版本已經夠快（Grain=16，§3 表格 Godot 估計同步＋分幀約 16ms），可能根本不需要做 Phase 5
- 只有實測證明同步阻塞造成明顯卡頓，才進 Phase 5

### Phase 5（進階，僅在 Phase 4 證明必要時才做）：真正 async readback
- 這是 `plan-ue5-migration.md` 設想的進階版本，`FRHIGPUBufferReadback::IsReady()` 逐幀 poll，CPU 端容忍「最多 1 幀舊」的資料
- 風險最高的部分：時序抓錯容易產生「格子悄悄消失/重疊」這種難以肉眼立刻發現的資料損壞，需要額外寫驗證工具（例如定期把 GPU 結果跟暫停 CPU 模擬做的影子計算比對）才容易抓到問題

---

## 五、待決策事項

1. **`ETileCategory`/`FTileCell::Category` 要不要真的填值，還是直接用 `EPhysicsCategory` 判斷夠了？**
   `EPhysicsCategory::Static` 的材質（石頭/泥土/木材）天生就不該進 GPU CA zone；`Powder`/`Liquid` 才需要。如果遊戲設計上「會進 CA 模擬的格子」跟「會擋碰撞的格子」這兩件事永遠跟 `EPhysicsCategory` 一致（沒有「Powder 但同時要當碰撞牆」這種特例），`ETileCategory` 這個獨立分類可能根本不需要，`InGpuZone`/`Upload` 直接查 `EPhysicsCategory` 就夠，省一個要手動維護的分類欄位。**需要先確認遊戲設計上有沒有這種特例**。
2. **GPU zone 跟著什麼移動？** 玩家角色位置最直覺，但要考慮：多人之後（目前計畫沒有 replication，暫時不用擔心）、玩家快速移動時 zone 整體重新上傳的瞬間成本。
3. **`Simulate()` 一幀呼叫幾次？** §2.2 提到 GPU 版視覺擴散速度天生比 CPU 慢（block 內限制一格 vs CPU 一次掃 3 格），需要實測調整，不是一次性決定，但要先有個起始值（建議先 1 次，量出實際視覺速度後再調）。
4. **手刻 RDG，還是借助 CompushadyUnreal？**（見 §3.1）手刻符合專案 C++-only 慣例、零外部依賴，但 RDG 細節要自己全部摸過；CompushadyUnreal（MIT、持續維護）能省下大半 buffer/UAV 綁定樣板碼，但它的文件主推 Blueprint 工作流，C++ API 細節這次沒查到，且會在專案裡多一個外部 plugin 依賴（裝這個插件本身是另一個需要先跟使用者確認的動作，不是這份計畫自動授權的）。**建議**：先讀它的 C++ 原始碼評估可行性，再決定，不要還沒讀程式碼就假設它好用。

---

## 六、檔案清單

**Phase 0 已新增（2026-06-21，驗證通過）：**
- `Plugins/VoxelWorld/Source/VoxelWorldShaders/`（新模組，`LoadingPhase=PostConfigInit`，理由見 Phase 0「踩到的坑」第 1 點）
  - `VoxelWorldShaders.Build.cs`
  - `Private/VoxelWorldShadersModule.cpp`（`AddShaderSourceDirectoryMapping`）
  - `Public/CaSpikeShader.h` + `Private/CaSpikeShader.cpp`（spike 用，Phase 1 會換成真正的 Margolus shader 類別）
  - `Public/M10Spike.h` + `Private/M10Spike.cpp`（spike 用 dispatch+讀回邏輯，含 §四 Phase 0 那 5 個坑的修正）
  - `Private/Tests/M10SpikeTest.cpp`（automation test，`VoxelWorldShaders.M10.GpuSpike`）
- `Plugins/VoxelWorld/Shaders/Private/CaSpike.usf`（spike shader 本體，Phase 1 會換成 `CaMargolus.usf`）
- `Plugins/VoxelWorld/VoxelWorld.uplugin`（新增 `VoxelWorldShaders` 模組項目）
- `Plugins/VoxelWorld/Source/VoxelWorld/VoxelWorld.Build.cs`（依賴 `VoxelWorldShaders`）

**Phase 1+ 待新增：**
- `Plugins/VoxelWorld/Shaders/Private/CaMargolus.usf`（真正的 Margolus CA shader，取代 spike 用的 `CaSpike.usf`）
- `Plugins/VoxelWorld/Source/VoxelWorldShaders/Public/CaMargolusShader.h` + `.cpp`（`DECLARE_GLOBAL_SHADER` 宣告，放在 `VoxelWorldShaders`，不是 `VoxelWorld`——理由同 Phase 0 踩坑第 1 點）
- `Plugins/VoxelWorld/Source/VoxelWorld/Private/CaGpuSimulator.cpp`（目前只有 `.h`，沒有 `.cpp`；填入時會呼叫 `VoxelWorldShaders` 公開的 dispatch 函式）

**Phase 3 待修改：**
- `Plugins/VoxelWorld/Source/VoxelWorld/Private/TileWorld3D.cpp`（`Tick()` 補 GPU 呼叫 + `InGpuZone()` 終於被使用）
- `Plugins/VoxelWorld/Source/VoxelWorld/Public/TileWorld3D.h`（如果需要新增「zone 跟誰走」的設定 API）
