#pragma once
#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

// M-10 Phase 1：Margolus block CA compute shader（Powder/Liquid 重力 + 液體橫向擴散）。
// 對應 Plugins/VoxelWorld/Shaders/Private/CaMargolus.usf，逐行對照 Godot CaGpuSimulator.cs
// 的 GLSL 邏輯翻譯（docs/plan-m10-gpu-ca.md §2/Phase 1）。
//
// 每次 Simulate 呼叫端要對 Phase=0 和 Phase=1 各 dispatch 一次（Margolus 棋盤格偏移），
// 對應 Godot CaGpuSimulator.cs:217-230 的 `for (int phase = 0; phase < 2; phase++)`。
// VOXELWORLDSHADERS_API 不能省：M10MargolusTest.cpp（呼叫端）在 VoxelWorld 模組，跟這個
// shader 類別所在的 VoxelWorldShaders 是不同的 DLL，沒有匯出宏會跨模組連結失敗
// （DECLARE_GLOBAL_SHADER 產生的 GetStaticType() 等符號找不到）。Phase 0 的 FCaSpikeCS
// 沒加這個宏沒事，是因為當時呼叫端跟類別宣告在同一個模組，這次踩到了才補上。
class VOXELWORLDSHADERS_API FCaMargolusCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FCaMargolusCS);
    SHADER_USE_PARAMETER_STRUCT(FCaMargolusCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, Cells)
        SHADER_PARAMETER(int32,  W)
        SHADER_PARAMETER(int32,  H)
        SHADER_PARAMETER(int32,  D)
        SHADER_PARAMETER(int32,  Phase)
        SHADER_PARAMETER(uint32, Rng)
        SHADER_PARAMETER(int32,  ZFrom)
        SHADER_PARAMETER(int32,  ZCount)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return true;
    }
};
