#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// M-10：shader 目錄註冊已搬到 VoxelWorldShaders 模組（LoadingPhase=PostConfigInit）。
// 原因：IMPLEMENT_GLOBAL_SHADER 在模組載入當下靜態註冊，這個 VoxelWorld 主模組是
// LoadingPhase=Default，太晚——shader type 註冊表已經鎖住，引擎會直接 assert 退出
// （Shader.h:1594）。見 VoxelWorldShadersModule.cpp 跟 docs/plan-m10-gpu-ca.md。
IMPLEMENT_MODULE(FDefaultGameModuleImpl, VoxelWorld)
