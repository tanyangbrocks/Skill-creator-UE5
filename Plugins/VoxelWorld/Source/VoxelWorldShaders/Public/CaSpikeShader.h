#pragma once
#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

// M-10 Phase 0：最小驗證 spike shader（docs/plan-m10-gpu-ca.md）。
// 每個 thread 把 index*2 寫進輸出 buffer，純粹驗證 RDG 編譯/dispatch/讀回路徑能跑，
// 不含任何 CA 邏輯。驗證完成後，Phase 1 會換成真正的 Margolus CA shader。
class FCaSpikeCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FCaSpikeCS);
    SHADER_USE_PARAMETER_STRUCT(FCaSpikeCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, OutputBuffer)
        SHADER_PARAMETER(uint32, Count)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return true;
    }
};
