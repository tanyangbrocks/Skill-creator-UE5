#include "M10GpuSimulatorTest.h"
#include "CaGpuSimulator.h"
#include "CaCellPacking.h"

bool M10GpuSimulatorTest::RunGravityTestAndVerify(FString& OutError)
{
    FCaGpuSimulator Sim;
    if (!Sim.Initialize())
    {
        OutError = TEXT("FCaGpuSimulator::Initialize 失敗（RHI 未初始化？是否用 -nullrhi 啟動）");
        return false;
    }

    const int32 W = 4, H = 4, D = 4;
    const int32 NumCells = W * H * D;
    auto Idx = [W, H](int32 x, int32 y, int32 z) { return z * H * W + y * W + x; };

    TArray<uint32> InitialData;
    InitialData.SetNumZeroed(NumCells);
    InitialData[Idx(0, 0, 0)] = CaCellPacking::Pack(static_cast<uint8>(EMaterialType::Sand));

    const bool bHasPhysics = Sim.Upload(InitialData, W, H, D);
    if (!bHasPhysics)
    {
        OutError = TEXT("Upload() 回傳 false，沒有偵測到 Powder/Liquid（打包格式錯誤？）");
        Sim.Release();
        return false;
    }

    Sim.Simulate(12345u);

    TMap<FIntVector, uint32> ChangedCells;
    Sim.Download([&ChangedCells](int32 x, int32 y, int32 z, uint32 PackedCell)
    {
        ChangedCells.Add(FIntVector(x, y, z), PackedCell);
    });

    bool bOk = true;

    if (const uint32* Origin = ChangedCells.Find(FIntVector(0, 0, 0)))
    {
        if (CaCellPacking::UnpackMaterialID(*Origin) != 0)
        {
            OutError = TEXT("(0,0,0) 變動了但不是空格");
            bOk = false;
        }
    }
    else
    {
        OutError = TEXT("(0,0,0) 沒有被標記為變動（dirty bit 沒設？）");
        bOk = false;
    }

    if (bOk)
    {
        if (const uint32* Fallen = ChangedCells.Find(FIntVector(0, 1, 0)))
        {
            if (CaCellPacking::UnpackMaterialID(*Fallen) != static_cast<uint8>(EMaterialType::Sand))
            {
                OutError = TEXT("(0,1,0) 變動了但不是 Sand");
                bOk = false;
            }
        }
        else
        {
            OutError = TEXT("(0,1,0) 沒有被標記為變動（Sand 沒有落下？）");
            bOk = false;
        }
    }

    Sim.Release();
    return bOk;
}
