#pragma once
#include "CoreMinimal.h"
#include "GridPos.h"

// ══════════════════════════════════════════════════════════════════════════
//  WorldScale — 世界尺度單一真相
//
//  ⚠️  規則：所有與 tile 大小有關的數值都必須在此推導，不得在其他地方硬寫。
//      改 TileSizeCm → Rebuild → 玩家/敵人/鏡頭/採掘全部自動跟進。
//
//  座標系對照：
//      Voxel  X = 水平（東）   Y = 從天頂向下（0=天頂, WorldH=地底）  Z = 深度（南）
//      UE5    X = 水平（東）   Y = 深度（南）                          Z = 向上
//
//  換算：
//      UE5 → Voxel：WorldToTile(FVector, WorldH)
//      Voxel → UE5：TileToWorld(X, Y, Z, WorldH)
// ══════════════════════════════════════════════════════════════════════════
namespace WorldScale
{
    // ── 基礎常數（唯一需要手動調整的地方）────────────────────────────────
    constexpr int32 ChunkSize          = 16;      // chunk 邊長（tile）
    constexpr int32 MegaChunkMult      = 4;       // Mega-Chunk = 4×4×4 chunks = 64³ tiles
    constexpr int32 PlayerW            = 1;       // 玩家寬度（tile）
    constexpr int32 PlayerH            = 2;       // 玩家高度（tile）
    constexpr float TileSizeCm         = 30.f;   // 每 tile 物理邊長（cm）← 改這個
    constexpr int32 DefaultWorldHeight = 256;     // 垂直 tile 上限（AVoxelWorldActor::WorldHeight 預設值）
    constexpr int32 GrainCurrent       = 1;       // 目前 Grain（M-1~M-9 用 1）
    constexpr int32 GrainTarget        = 64;      // 長期目標（M-10 GPU CA 後）

    // ── 衍生常數：玩家碰撞膠囊 ────────────────────────────────────────
    // 半徑比 tile 略窄（×0.45）使玩家能緊貼牆壁移動而不卡住
    constexpr float CapsuleRadius      = TileSizeCm * PlayerW * 0.45f;
    constexpr float CapsuleHalfHeight  = TileSizeCm * PlayerH * 0.5f;

    // ── 衍生常數：鏡頭彈簧臂 ──────────────────────────────────────────
    constexpr float CameraArmLength    = TileSizeCm * 20.f;  // 20 tile 距離
    constexpr float CameraArmZOffset   = TileSizeCm * 1.5f;  // 臂根向上 1.5 tile

    // ── 衍生常數：移動 ────────────────────────────────────────────────
    constexpr float WalkSpeedCm        = TileSizeCm * 20.f;  // 20 tile/s
    constexpr float JumpZVelocityCm    = TileSizeCm * 14.f;  // 抵達 ~3 tile 高度（物理公式推導）
    // 注意：UE5 重力 980 cm/s² 未依 tile 縮放。若 GrainTarget 改變需同時調整
    // GetCharacterMovement()->GravityScale = (TileSizeCm / 30.f)（保持 tile/s² 一致）

    // ── 座標轉換：Voxel → UE5 FVector ────────────────────────────────
    inline FVector TileToWorld(int32 VoxX, int32 VoxY, int32 VoxZ,
                                int32 WorldH = DefaultWorldHeight)
    {
        return FVector(
            static_cast<float>(VoxX)          * TileSizeCm,  // voxX  → UE5 X
            static_cast<float>(VoxZ)          * TileSizeCm,  // voxZ  → UE5 Y
            static_cast<float>(WorldH - VoxY) * TileSizeCm   // voxY  → UE5 Z（反轉）
        );
    }

    inline FVector TileToWorld(const FGridPos& P, int32 WorldH = DefaultWorldHeight)
    {
        return TileToWorld(P.X, P.Y, P.Z, WorldH);
    }

    // ── 座標轉換：UE5 FVector → Voxel FGridPos（整數，用於 gameplay 查格）──
    inline FGridPos WorldToTile(const FVector& UELoc, int32 WorldH = DefaultWorldHeight)
    {
        const float InvS = 1.f / TileSizeCm;
        return FGridPos(
            FMath::RoundToInt(UELoc.X * InvS),
            FMath::RoundToInt(static_cast<float>(WorldH) - UELoc.Z * InvS),
            FMath::RoundToInt(UELoc.Y * InvS)
        );
    }

    // ── 座標轉換：UE5 FVector → Voxel FVector（浮點，用於射線/插值）────
    inline FVector WorldToTileF(const FVector& UELoc, int32 WorldH = DefaultWorldHeight)
    {
        const float InvS = 1.f / TileSizeCm;
        return FVector(
            UELoc.X * InvS,
            static_cast<float>(WorldH) - UELoc.Z * InvS,
            UELoc.Y * InvS
        );
    }

    // ── 方向向量轉換：UE5 方向 → Voxel 方向（不乘尺度，只轉軸）────────
    inline FVector DirToVoxel(const FVector& UEDir)
    {
        return FVector(UEDir.X, -UEDir.Z, UEDir.Y);
    }
}
