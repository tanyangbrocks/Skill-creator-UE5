#pragma once
#include "CoreMinimal.h"

// M-10：GPU Compute Shader CA 模擬器（對應 Godot CaGpuSimulator.cs）
// 目前實作為 stub（IsAvailable=false），TileWorld3D 會走 CPU CA 路徑。
// M-10 正式實作時填入 RDG Compute Shader + 非同步 readback。
class VOXELWORLD_API FCaGpuSimulator
{
public:
    // GPU 模擬區尺寸（需與 WorldScale.Grain 保持一致）
    static constexpr int32 ZoneW = 128;
    static constexpr int32 ZoneH = 256;
    static constexpr int32 ZoneD = 128;

    FCaGpuSimulator() = default;
    ~FCaGpuSimulator();

    // 初始化 GPU 資源（shader / buffer）；失敗後 IsAvailable() 回傳 false
    bool Initialize();

    // GPU 是否可用（RDG Compute Shader 初始化成功）
    bool IsAvailable() const { return bAvailable; }

    // 更新模擬中心座標（玩家移動時呼叫）
    void SetOrigin(int32 OX, int32 OY, int32 OZ);

    // 取得模擬中心
    int32 GetOriginX() const { return OriginX; }
    int32 GetOriginY() const { return OriginY; }
    int32 GetOriginZ() const { return OriginZ; }

    // 上傳世界 tile 資料到 GPU（Dispatch 前呼叫）
    // Data：ZoneW × ZoneH × ZoneD 個 uint8（MaterialID）
    void Upload(const TArray<uint8>& Data);

    // 執行一幀 GPU CA 模擬
    void Simulate();

    // 非同步讀回：將 GPU 結果以 SetCellCb(x, y, z, materialByte) 回呼
    using FSetCellCallback = TFunction<void(int32, int32, int32, uint8)>;
    void Download(FSetCellCallback SetCellCb);

    // 釋放 GPU 資源
    void Release();

private:
    bool  bAvailable = false;
    int32 OriginX = 0;
    int32 OriginY = 0;
    int32 OriginZ = 0;

    // M-10 正式實作：FRDGBuilder / Structured Buffer / Compute Shader 等資源放這裡
};
