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
    // ⚠️ GrainCurrent 必須在 PlayerW/PlayerH/TileSizeCm 之前宣告（constexpr 前向引用限制）
    // 對應 Godot WorldScale.cs Grain=16：1 遊戲單位 = 16 tile。
    // 改此值 → Rebuild → 玩家體型/TileSize/採掘範圍/形狀半徑全部自動跟進。
    constexpr int32 GrainCurrent       = 16;     // 目前 Grain（對應 Godot WorldScale.cs Grain=16）
    constexpr int32 GrainTarget        = 64;      // 長期目標（M-10 GPU CA 後）
    constexpr int32 PlayerW            = GrainCurrent;          // 玩家寬度（tile）= 1 遊戲單位
    constexpr int32 PlayerH            = GrainCurrent * 2;      // 玩家高度（tile）= 2 遊戲單位 = 200cm @ Grain=16
    constexpr float TileSizeCm         = 100.f / static_cast<float>(GrainCurrent);  // 每 tile 邊長（cm）= 1m / Grain
    constexpr int32 DefaultWorldHeight = 256;     // 垂直 tile 上限（AVoxelWorldActor::WorldHeight 預設值）

    // ── 衍生常數：玩家碰撞膠囊 ────────────────────────────────────────
    // 半徑比 tile 略窄（×0.45）使玩家能緊貼牆壁移動而不卡住
    constexpr float CapsuleRadius      = TileSizeCm * PlayerW * 0.45f;
    constexpr float CapsuleHalfHeight  = TileSizeCm * PlayerH * 0.5f;

    // ── 衍生常數：鏡頭彈簧臂 ──────────────────────────────────────────
    constexpr float CameraArmLength    = TileSizeCm * 20.f;  // 20 tile 距離
    constexpr float CameraArmZOffset   = TileSizeCm * 1.5f;  // 臂根向上 1.5 tile

    // ── 衍生常數：移動 ────────────────────────────────────────────────
    // WalkSpeed：以「遊戲單位/s」表示（1 遊戲單位 = GrainCurrent tiles = 100 cm）。
    // TileSizeCm * GrainCurrent = 100 cm（恆等式），故 4 遊戲單位/s = 400 cm/s（不隨 Grain 改變）。
    constexpr float WalkSpeedCm        = TileSizeCm * static_cast<float>(GrainCurrent) * 4.f;  // ≡ 400 cm/s
    // Jump：GravityScale = TileSizeCm / 30.f（ASkillCreatorCharacter 建構子設定），
    // 有效重力 g_eff = 980 * (TileSizeCm/30)，JumpZVelocity = TileSizeCm * 14 → 抵達 3 tile 高。
    // 3 tiles = 3 * 6.25 cm = 18.75 cm（Grain=16；tile 單位跳躍高度不隨 Grain 改變）。
    constexpr float JumpZVelocityCm    = TileSizeCm * 14.f;  // 配合 GravityScale 使用：3 tile 高
    // GravityScale 縮放因子（讓 980 cm/s² 等效為「tile 單位重力」一致）
    constexpr float GravityScaleMult   = TileSizeCm / 30.f;

    // ── 衍生常數：採掘 / 放置（對應 Godot PlayerController.cs MiningRange / Main.cs _shapeRadius）─
    // Godot: MiningRange => BodyH * 6f（PlayerController.cs:42）；BodyH 預設 = WorldScale.PlayerH。
    // UE5 用自己的 PlayerH（tile 高度）重新推導，不照抄 Godot 的 32/6 數字。
    constexpr int32 MiningRangeTiles   = PlayerH * 6;                          // ~6× 玩家高度（tile）
    // Godot: _shapeRadius = WorldScale.PlayerH/6（Main.cs:72），下限 1。
    constexpr int32 DefaultShapeRadius = (PlayerH / 6 > 0) ? (PlayerH / 6) : 1; // 預設形狀半徑（tile）

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
