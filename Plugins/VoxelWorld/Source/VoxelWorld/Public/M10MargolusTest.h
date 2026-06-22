#pragma once
#include "CoreMinimal.h"

// M-10 Phase 1：驗證 Margolus CA shader 邏輯翻譯正確（docs/plan-m10-gpu-ca.md Phase 1）。
// 純驗證用，不是正式功能——FCaGpuSimulator 正式實作（Phase 2）會用同一套 shader，
// 但走 Upload/Simulate/Download 公開 API，不是這裡的 ad-hoc 一次性 dispatch。
namespace M10MargolusTest
{
    // 在一個 4x4x4 全 Air 的 zone 裡，(0,0,0) 放一顆 Sand，dispatch 一次完整 Simulate
    // （Phase 0 + Phase 1），驗證 Sand 是否落到 (0,1,0)（Pass 1 直接下落）。
    // 這個案例刻意設計成跟亂數處理順序無關（只有一個 mobile cell，沒有競爭對象），
    // 確保結果是確定性的、好驗證的。
    VOXELWORLD_API bool RunGravityTestAndVerify(FString& OutError);
}
