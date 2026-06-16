#pragma once
#include "CoreMinimal.h"
#include "GridPos.h"
#include "ElementType.h"
#include "CharacterStats.h"

// ── Aura 快照（對應 Godot AuraSnapshot.cs）───────────────────────────────

struct FAuraEntryData
{
    ESkillElementType Element  = ESkillElementType::None;
    float             Duration = 0.f;
};

struct FAuraEffectData
{
    FName EffectType        = NAME_None;
    float RemainingDuration = 0.f;
    float AccumulatedState  = 0.f;  // 僅 FQuicksandSlowEffect 使用
};

struct FAuraSnapshot
{
    TArray<FAuraEntryData>  Entries;
    TArray<FAuraEffectData> Effects;
};

// ── 角色動態狀態快照（對應 Godot CharStateSnapshot.cs W-5b）───────────────

struct FCharStateSnapshot
{
    float Stamina      = 100.f;
    float MentalEnergy = 100.f;
    float Mood         = 70.f;
    float BodyTemp     = 36.5f;
    float Thirst       = 100.f;
    float Hunger       = 100.f;
    float Oxygen       = 100.f;
};

// ── 單一實體快照（對應 Godot EntitySnapshot.cs）──────────────────────────

struct FEntitySnapshot
{
    static constexpr int32 PlayerEntityId = -1;

    int32    EntityId  = 0;
    FGridPos Position  = {};
    float    Hp        = 0.f;
    float    Mp        = 0.f;
    bool     bWasAlive = true;

    FAuraSnapshot Aura;

    bool             bHasCharState = false;
    FCharStateSnapshot CharState;

    bool            bHasCharStats = false;
    FCharacterStats CharStats;
};

// ── Tile 世界快照（對應 Godot S-13 TileWorldSnapshot）───────────────────────
// 儲存玩家周圍球形範圍內的 tile 材質（uint8 = EMaterialType，避免引入 VoxelWorld 依賴）
struct FTileWorldSnapshot
{
    TMap<FGridPos, uint8> Tiles; // GridPos → EMaterialType as uint8
};

// ── 世界快照（對應 Godot WorldSnapshot.cs）────────────────────────────────

struct FWorldSnapshot
{
    TArray<FEntitySnapshot> Entities;
    FTileWorldSnapshot      TileSnap; // S-13：玩家周圍球形 tile 快照（radius 由施法者指定）
};
