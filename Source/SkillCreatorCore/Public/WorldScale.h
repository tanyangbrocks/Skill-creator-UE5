#pragma once

// 世界座標與尺度常數，所有 Module 共用
namespace WorldScale
{
    // Chunk 邊長（每個 Chunk 存 ChunkSize³ 個 FTileCell）
    constexpr int32 ChunkSize    = 16;

    // Mega-Chunk = MegaChunkMult × MegaChunkMult × MegaChunkMult 個 Chunk
    // 對應一個 RMC Section（M-6 渲染用）
    constexpr int32 MegaChunkMult = 4;   // 4×4×4 Chunks = 64³ tiles per Section

    // 玩家佔用格數（Grain 1 基準）
    constexpr int32 PlayerW      = 1;
    constexpr int32 PlayerH      = 2;

    // 每個 tile 的物理邊長（公分；Grain 1 = 30cm）
    constexpr float TileSizeCm   = 30.f;

    // 目前開發 Grain（M-1~M-9 用 1，M-10 GPU CA 目標 64）
    constexpr int32 GrainCurrent = 1;
    constexpr int32 GrainTarget  = 64;
}
