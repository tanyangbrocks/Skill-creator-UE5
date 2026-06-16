#include "CaGpuSimulator.h"

// M-10 stub：GPU 不可用，所有方法為 no-op。
// 正式實作時替換 Initialize() 以建立 RDG Compute Pipeline，
// Simulate() 以 GraphBuilder.AddPass 分派，Download() 以 FRHIGPUFence 等待 readback。

FCaGpuSimulator::~FCaGpuSimulator()
{
    Release();
}

bool FCaGpuSimulator::Initialize()
{
    // M-10：建立 RDG Structured Buffer、載入 HLSL Compute Shader、設定 readback fence
    bAvailable = false;
    return false;
}

void FCaGpuSimulator::SetOrigin(int32 OX, int32 OY, int32 OZ)
{
    OriginX = OX;
    OriginY = OY;
    OriginZ = OZ;
}

void FCaGpuSimulator::Upload(const TArray<uint8>& Data)
{
    // M-10：FRHICommandListImmediate::UpdateBuffer
}

void FCaGpuSimulator::Simulate()
{
    // M-10：FRDGBuilder::AddPass → Dispatch(ZoneW/8, ZoneH/8, ZoneD/8)
}

void FCaGpuSimulator::Download(FSetCellCallback SetCellCb)
{
    // M-10：FRHIGPUFence 等待 → 遍歷 readback buffer → SetCellCb(x, y, z, byte)
}

void FCaGpuSimulator::Release()
{
    bAvailable = false;
    // M-10：釋放 FRDGBuffer / Fence
}
