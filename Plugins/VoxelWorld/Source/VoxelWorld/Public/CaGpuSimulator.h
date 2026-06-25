#pragma once
#include "CoreMinimal.h"
#include "RenderGraphFwd.h"
#include <atomic>

// 前向宣告：只存指標，不需要 include RHIGPUReadback.h（避免非 Render 模組引用時找不到）
class FRHIGPUBufferReadback;

// M-10：GPU Compute Shader CA 模擬器（對應 Godot CaGpuSimulator.cs，逐行對照翻譯見
// docs/plan-m10-gpu-ca.md）。Phase 2：同步阻塞版（對應 Godot 的 Submit()+Sync()，
// 不是非同步），先求正確，Phase 5 才考慮升級成真正 async。
//
// ⚠️ 2026-06-21：Upload/Download 的資料格式從最初 stub 設想的「ZoneW×ZoneH×ZoneD 個
// uint8 (MaterialID)」改成「打包過的 32-bit cell」（CaCellPacking::Pack 格式，bits 0-7
// MaterialType / 8-9 physCompact / 10-13 density4 / 14 dirty / 16-23 Timer / 24-30
// Variant）——這是逐行讀過 Godot CaGpuSimulator.cs 後確認的真實格式，純 MaterialID byte
// 不夠：shader 需要 physCompact/density 才能判斷 Powder/Liquid 移動規則。此時還沒有任何
// 呼叫端用到舊簽名（TileWorld3D::Tick() 還沒接 GPU CA，見 Phase 3），改簽名不影響既有程式碼。
class VOXELWORLD_API FCaGpuSimulator
{
public:
    // GPU 模擬區尺寸（需與 WorldScale.Grain 保持一致）
    static constexpr int32 ZoneW = 128;
    static constexpr int32 ZoneH = 256;
    static constexpr int32 ZoneD = 128;

    FCaGpuSimulator() = default;
    ~FCaGpuSimulator();

    // 初始化 GPU 資源；失敗後 IsAvailable() 回傳 false（例如 -nullrhi 啟動）
    bool Initialize();

    // GPU 是否可用
    bool IsAvailable() const { return bAvailable; }

    // 更新模擬中心座標（玩家移動時呼叫）
    void SetOrigin(int32 OX, int32 OY, int32 OZ);

    int32 GetOriginX() const { return OriginX; }
    int32 GetOriginY() const { return OriginY; }
    int32 GetOriginZ() const { return OriginZ; }

    // 上傳已打包的 cell 資料到 GPU（CaCellPacking::Pack 格式）。InW/InH/InD 是這次上傳的
    // zone 尺寸（不一定要等於 ZoneW/H/D，Phase 2 單元測試用小尺寸，Phase 3 接 TileWorld3D
    // 才會用滿 ZoneW/H/D）。
    // 回傳 true = zone 內有 Powder/Liquid，需要呼叫 Simulate+Download；
    // 回傳 false = 全部靜態，可以跳過（對應 Godot Upload() 的 hasPhysics 回傳值）。
    bool Upload(const TArray<uint32>& PackedCells, int32 InW, int32 InH, int32 InD);

    // 執行一次完整 Simulate（內部對 Phase 0+1 各 dispatch 一次，對應 Margolus CA 一輪——
    // 跟 Godot CaGpuSimulator.cs:217-230 的 for phase in 0..2 同義）。
    void Simulate(uint32 RngSeed);

    // 同步讀回（阻塞，Phase 2 範圍內可接受），只有 dirty bit 被設的格子才會呼叫 SetCellCb。
    // 座標是 zone-local（0..InW-1 等），不是世界座標，呼叫端自己加上 Origin 偏移。
    using FSetCellCallback = TFunction<void(int32 LocalX, int32 LocalY, int32 LocalZ, uint32 PackedCell)>;
    void Download(FSetCellCallback SetCellCb);

    // ── Phase 4：Stat Overlay ──────────────────────────────────
    // PIE console 下 `stat quick` 可看到各步驟 CPU 時間（QUICK_SCOPE_CYCLE_COUNTER → STATGROUP_Quick）。
    // 具體 ms 值每 10 秒從 TileWorld3D::TickGpuZone UE_LOG 輸出。

    // ── Phase 5：1-frame latency async readback ────────────────
    // BeginAsync：非同步踢 Upload+Simulate+ReadbackCopy，不阻塞 game thread。
    // 若 zone 全靜態（無 Powder/Liquid），回傳 false，不踢 GPU 工作。
    // PackedCells 以值傳遞——內部 move 進 lambda，呼叫端用 MoveTemp() 傳入。
    bool BeginAsync(TArray<uint32> PackedCells, int32 InW, int32 InH, int32 InD, uint32 RngSeed);

    // TryCollectAsync：收取上一幀 BeginAsync 踢的結果，若就緒則呼叫 WorldCellCb（world coords）。
    // 回傳 true = 結果已收取並寫入世界；false = GPU 尚未就緒（通常下一幀就好了）。
    using FWorldCellCb = TFunction<void(int32 wx, int32 wy, int32 wz, uint32 Cell)>;
    bool TryCollectAsync(FWorldCellCb WorldCellCb);

    // 是否有正在飛行中的 async readback（BeginAsync 踢了但 TryCollectAsync 尚未收取）
    bool HasPendingAsync() const;

    // 釋放 GPU 資源
    void Release();

private:
    bool  bAvailable = false;
    int32 OriginX = 0;
    int32 OriginY = 0;
    int32 OriginZ = 0;

    // 目前已上傳的 zone 尺寸（Download 算 buffer 大小、座標反推用）
    int32 CurW = 0;
    int32 CurH = 0;
    int32 CurD = 0;

    // 跨 Upload/Simulate/Download 呼叫持續存在的 GPU buffer（每次呼叫都是獨立的
    // ENQUEUE_RENDER_COMMAND，靠這個 pooled buffer 把同一份 GPU 資料接到下一次呼叫，
    // 見 docs/plan-m10-gpu-ca.md Phase 2）。
    TRefCountPtr<FRDGPooledBuffer> PooledBuffer;

    // ── Phase 5 async state ───────────────────────────────────
    // AsyncReadbackInFlight：render thread 建立後寫入此 atomic，game thread poll。
    // 只有 BeginAsync/TryCollectAsync 的呼叫者（game thread）負責讀寫 non-atomic 成員；
    // AsyncReadbackInFlight 本身用 acquire/release 保證 game↔render 可見性。
    struct FAsyncCell { int32 WX, WY, WZ; uint32 Packed; };
    std::atomic<FRHIGPUBufferReadback*> AsyncReadbackInFlight{nullptr};
    TArray<FAsyncCell> AsyncCells;       // render thread 填 → FlushRenderingCommands → game thread 讀
    int32 AsyncCornerX = 0;             // BeginAsync 時由 game thread 設，OriginX - InW/2
    int32 AsyncCornerY = 0;
    int32 AsyncCornerZ = 0;
    int32 AsyncW       = 0;
    int32 AsyncH       = 0;
    int32 AsyncD       = 0;
};
