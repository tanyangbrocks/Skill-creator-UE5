#include "CaSpikeShader.h"

// 檔案實際放在 Plugins/VoxelWorld/Shaders/Private/CaSpike.usf——shader 目錄映射只把
// "Shaders/" 對應到 "/Plugin/VoxelWorld"，"Private" 子資料夾仍是虛擬路徑的一部分
// （跟 AppleARKit 的 "/AppleARKit/Private/ColorSpaceConversion.usf" 同一個慣例）。
IMPLEMENT_GLOBAL_SHADER(FCaSpikeCS, "/Plugin/VoxelWorld/Private/CaSpike.usf", "MainCS", SF_Compute);
