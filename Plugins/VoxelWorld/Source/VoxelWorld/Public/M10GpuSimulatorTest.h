#pragma once
#include "CoreMinimal.h"

// M-10 Phase 2：驗證 FCaGpuSimulator 公開 API（Initialize→Upload→Simulate→Download）正確，
// 特別是三個分開呼叫之間 GPU buffer 持久化機制（TRefCountPtr<FRDGPooledBuffer>）有沒有接好
// （docs/plan-m10-gpu-ca.md Phase 2）。跟 Phase 1 的 M10MargolusTest 用同一個重力測試案例，
// 但這次走真正的公開 API，不是直接 dispatch shader。
namespace M10GpuSimulatorTest
{
    VOXELWORLD_API bool RunGravityTestAndVerify(FString& OutError);
}
