#include "CaMargolusShader.h"

// 檔案實際放在 Plugins/VoxelWorld/Shaders/Private/CaMargolus.usf——理由同 CaSpikeShader.cpp
// 的註解：shader 目錄映射只把 "Shaders/" 對應到 "/Plugin/VoxelWorld"，"Private" 子資料夾
// 仍是虛擬路徑的一部分。
IMPLEMENT_GLOBAL_SHADER(FCaMargolusCS, "/Plugin/VoxelWorld/Private/CaMargolus.usf", "MainCS", SF_Compute);
