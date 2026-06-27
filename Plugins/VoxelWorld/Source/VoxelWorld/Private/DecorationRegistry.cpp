#include "DecorationRegistry.h"

TArray<FDecorationDef> FDecorationRegistry::Defs;
bool FDecorationRegistry::bInitialized = false;

// ============================================================
// DECO-1：初始裝飾登錄
// 新增裝飾物件 SOP：
//   1. 在 UE Editor 匯入 FBX 到 /Game/Decorations/SM_YourProp（一次性）
//   2. 在此函式加一行 Register({...})
//   3. Build（.cpp 改，可 Live Coding）
// ============================================================

void FDecorationRegistry::Initialize()
{
    if (bInitialized) return;
    bInitialized = true;
    Defs.Empty();

    // ── 小石頭（草地 / 乾土）────────────────────────────────────────────
    {
        FDecorationDef D;
        D.PropId = "Rock_Small";
        D.MeshPath = FSoftObjectPath(TEXT("/Game/Decorations/SM_Rock_Small.SM_Rock_Small"));
        D.ValidSurfaces.Add(EMaterialType::Grass);
        D.ValidSurfaces.Add(EMaterialType::Dirt_Dry);
        D.Density      = 0.04f;
        D.ScaleRange   = FVector2D(0.8f, 1.4f);
        D.MaxSlopeDeg  = 30.f;
        Register(D);
    }

    // ── 蘑菇（草地）──────────────────────────────────────────────────────
    {
        FDecorationDef D;
        D.PropId = "Mushroom_Red";
        D.MeshPath = FSoftObjectPath(TEXT("/Game/Decorations/SM_Mushroom_Red.SM_Mushroom_Red"));
        D.ValidSurfaces.Add(EMaterialType::Grass);
        D.Density      = 0.02f;
        D.ScaleRange   = FVector2D(0.6f, 1.2f);
        D.MaxSlopeDeg  = 15.f;
        Register(D);
    }

    // ── 仙人掌（沙地）────────────────────────────────────────────────────
    {
        FDecorationDef D;
        D.PropId = "Cactus";
        D.MeshPath = FSoftObjectPath(TEXT("/Game/Decorations/SM_Cactus.SM_Cactus"));
        D.ValidSurfaces.Add(EMaterialType::Sand);
        D.Density      = 0.06f;
        D.ScaleRange   = FVector2D(1.0f, 1.8f);
        D.MaxSlopeDeg  = 10.f;
        Register(D);
    }

    // ── 魔法水晶（石頭地表）──────────────────────────────────────────────
    {
        FDecorationDef D;
        D.PropId = "Crystal_Blue";
        D.MeshPath = FSoftObjectPath(TEXT("/Game/Decorations/SM_Crystal_Blue.SM_Crystal_Blue"));
        D.ValidSurfaces.Add(EMaterialType::Stone_Cobble);
        D.Density      = 0.015f;
        D.ScaleRange   = FVector2D(0.5f, 1.5f);
        D.MaxSlopeDeg  = 45.f;
        Register(D);
    }
}

void FDecorationRegistry::Register(const FDecorationDef& Def)
{
    Defs.Add(Def);
}

const TArray<FDecorationDef>& FDecorationRegistry::GetAll()
{
    return Defs;
}
