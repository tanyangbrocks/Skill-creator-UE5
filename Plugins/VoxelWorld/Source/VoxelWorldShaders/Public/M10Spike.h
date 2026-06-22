#pragma once
#include "CoreMinimal.h"

// M-10 Phase 0：驗證 RDG compute shader 基本機制在這個專案能跑（docs/plan-m10-gpu-ca.md）。
// 純驗證用，不是正式功能——Phase 0 確認可行後，這個檔案會被刪除，邏輯併入 FCaGpuSimulator。
namespace M10Spike
{
    // 同步執行 spike shader + 讀回（會阻塞呼叫端直到 GPU 完成，驗證用不考慮效能），
    // 驗證每個輸出格是否等於 index*2。成功回 true；失敗時 OutError 填入原因。
    VOXELWORLDSHADERS_API bool RunSpikeAndVerify(int32 Count, FString& OutError);
}
